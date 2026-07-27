#include "libvideo2x.h"

extern "C" {
#include <libavutil/avutil.h>
}

#include <spdlog/spdlog.h>

#include "avutils.h"
#include "decoder.h"
#include "encoder.h"
#include "logger_manager.h"
#include "processor.h"
#include "processor_factory.h"
#include "resume_session.h"

namespace video2x {

VideoProcessor::VideoProcessor(
    const processors::ProcessorConfig proc_cfg,
    const encoder::EncoderConfig enc_cfg,
    const uint32_t vk_device_idx,
    const AVHWDeviceType hw_device_type,
    const bool benchmark
)
    : proc_cfg_(proc_cfg),
      enc_cfg_(enc_cfg),
      vk_device_idx_(vk_device_idx),
      hw_device_type_(hw_device_type),
      benchmark_(benchmark) {}

[[gnu::target_clones("arch=x86-64-v4", "arch=x86-64-v3", "default")]]
int VideoProcessor::process(
    const std::filesystem::path in_fname,
    const std::filesystem::path out_fname
) {
    return process_internal(in_fname, out_fname, nullptr);
}

int VideoProcessor::process_resumable(
    const std::filesystem::path in_fname,
    const std::filesystem::path out_fname,
    const std::filesystem::path resume_artifact
) {
    std::filesystem::path artifact = resume_artifact;
    if (artifact.empty()) {
        artifact = out_fname;
        artifact += resume::ARTIFACT_EXTENSION;
    }
    resume::Session session(
        artifact, in_fname, out_fname, proc_cfg_, enc_cfg_, vk_device_idx_, hw_device_type_
    );
    int ret = process_internal(in_fname, session.temporary_output_path(), &session);
    if (ret < 0) {
        return ret;
    }
    std::string error;
    ret = session.publish_output(session.temporary_output_path(), error);
    if (ret < 0) {
        logger()->critical("Failed to publish final output: {}", error);
        state_.store(VideoProcessorState::Failed);
        return ret;
    }
    state_.store(VideoProcessorState::Completed);
    return 0;
}

int VideoProcessor::process_internal(
    const std::filesystem::path& in_fname,
    const std::filesystem::path& out_fname,
    resume::Session* resume_session
) {
    int ret = 0;
    frame_idx_.store(0);
    recovered_frames_.store(0);
    total_frames_.store(0);
    encoded_frame_idx_ = 0;
    source_frame_idx_ = 0;
    resume_session_ = resume_session;

    // Helper lambda to handle errors:
    auto handle_error = [&](int error_code, const std::string& msg) {
        // Format and log the error message
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(error_code, errbuf, sizeof(errbuf));
        logger()->critical("{}: {}", msg, errbuf);

        // Set the video processor state to failed and return the error code
        state_.store(VideoProcessorState::Failed);
        return error_code;
    };

    // Set the video processor state to running
    state_.store(VideoProcessorState::Running);

    // Create a smart pointer to manage the hardware device context
    std::unique_ptr<AVBufferRef, decltype(&avutils::av_bufferref_deleter)> hw_ctx(
        nullptr, &avutils::av_bufferref_deleter
    );

    // Initialize hardware device context
    if (hw_device_type_ != AV_HWDEVICE_TYPE_NONE) {
        AVBufferRef* tmp_hw_ctx = nullptr;
        ret = av_hwdevice_ctx_create(&tmp_hw_ctx, hw_device_type_, nullptr, nullptr, 0);
        if (ret < 0) {
            return handle_error(ret, "Error initializing hardware device context");
        }
        hw_ctx.reset(tmp_hw_ctx);
    }

    // Initialize input decoder
    decoder::Decoder decoder;
    ret = decoder.init(hw_device_type_, hw_ctx.get(), in_fname);
    if (ret < 0) {
        return handle_error(ret, "Failed to initialize decoder");
    }

    AVFormatContext* ifmt_ctx = decoder.get_format_context();
    AVCodecContext* dec_ctx = decoder.get_codec_context();
    int in_vstream_idx = decoder.get_video_stream_index();

    if (resume_session_ != nullptr) {
        std::string error;
        ret = resume_session_->initialize(ifmt_ctx, dec_ctx, in_vstream_idx, error);
        if (ret < 0) {
            logger()->critical("Failed to initialize resume workspace: {}", error);
            state_.store(VideoProcessorState::Failed);
            return ret;
        }
        frame_idx_.store(resume_session_->completed_output_frames());
        recovered_frames_.store(resume_session_->completed_output_frames());
    }

    // Create and initialize the appropriate filter
    std::unique_ptr<processors::Processor> processor(
        processors::ProcessorFactory::instance().create_processor(proc_cfg_, vk_device_idx_)
    );
    if (processor == nullptr) {
        return handle_error(-1, "Failed to create filter instance");
    }

    // Initialize output dimensions based on filter configuration
    int output_width = 0, output_height = 0;
    processor->get_output_dimensions(
        proc_cfg_, dec_ctx->width, dec_ctx->height, output_width, output_height
    );
    if (output_width <= 0 || output_height <= 0) {
        return handle_error(-1, "Failed to determine the output dimensions");
    }

    // Initialize the encoder
    encoder::Encoder encoder;
    ret = encoder.init(
        hw_ctx.get(),
        out_fname,
        ifmt_ctx,
        dec_ctx,
        enc_cfg_,
        output_width,
        output_height,
        proc_cfg_.frm_rate_mul,
        in_vstream_idx
    );
    if (ret < 0) {
        return handle_error(ret, "Failed to initialize encoder");
    }

    // Initialize the filter
    ret = processor->init(dec_ctx, encoder.get_encoder_context(), hw_ctx.get());
    if (ret < 0) {
        return handle_error(ret, "Failed to initialize filter");
    }

    // Process frames using the encoder and decoder
    ret = process_frames(decoder, encoder, processor);
    if (ret < 0) {
        if (ret == AVERROR_EXIT && state_.load() == VideoProcessorState::Aborted) {
            return ret;
        }
        return handle_error(ret, "Error processing frames");
    }

    // Write the output file trailer
    ret = av_write_trailer(encoder.get_format_context());
    if (ret < 0) {
        return handle_error(ret, "Error writing output file trailer");
    }

    // Check if an error occurred during processing
    if (ret < 0 && ret != AVERROR_EOF) {
        return handle_error(ret, "Error occurred");
    }

    // Processing has completed successfully
    if (resume_session_ == nullptr) {
        state_.store(VideoProcessorState::Completed);
    }
    return 0;
}

// Process frames using the selected filter.
int VideoProcessor::process_frames(
    decoder::Decoder& decoder,
    encoder::Encoder& encoder,
    std::unique_ptr<processors::Processor>& processor
) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    int ret = 0;

    // Get required objects
    AVFormatContext* ifmt_ctx = decoder.get_format_context();
    AVCodecContext* dec_ctx = decoder.get_codec_context();
    int in_vstream_idx = decoder.get_video_stream_index();
    AVFormatContext* ofmt_ctx = encoder.get_format_context();
    AVCodecContext* enc_ctx = encoder.get_encoder_context();
    int* stream_map = encoder.get_stream_map();
    bool processed_new_source = false;

    auto clear_checkpoint_frames = [&]() {
        for (AVFrame* checkpoint_frame : checkpoint_frames_) {
            av_frame_free(&checkpoint_frame);
        }
        checkpoint_frames_.clear();
    };

    // Reference to the previous frame does not require allocation
    // It will be cloned from the current frame
    std::unique_ptr<AVFrame, decltype(&avutils::av_frame_deleter)> prev_frame(
        nullptr, &avutils::av_frame_deleter
    );

    // Allocate space for the decoded frames
    std::unique_ptr<AVFrame, decltype(&avutils::av_frame_deleter)> frame(
        av_frame_alloc(), &avutils::av_frame_deleter
    );
    if (frame == nullptr) {
        logger()->critical("Error allocating frame");
        return AVERROR(ENOMEM);
    }

    // Allocate space for the decoded packets
    std::unique_ptr<AVPacket, decltype(&avutils::av_packet_deleter)> packet(
        av_packet_alloc(), &avutils::av_packet_deleter
    );
    if (packet == nullptr) {
        logger()->critical("Error allocating packet");
        return AVERROR(ENOMEM);
    }

    // Set the total number of frames in the VideoProcessingContext
    logger()->debug("Estimating the total number of frames to process");
    total_frames_ = avutils::get_video_frame_count(ifmt_ctx, in_vstream_idx);

    if (total_frames_ <= 0) {
        logger()->warn("Unable to determine the total number of frames");
        total_frames_ = 0;
    } else {
        logger()->debug("{} frames to process", total_frames_.load());
    }

    // Set total frames for interpolation
    if (processor->get_processing_mode() == processors::ProcessingMode::Interpolate) {
        int64_t source_frames = total_frames_.load();
        total_frames_.store(
            source_frames > 0
                ? source_frames + (source_frames - 1) * (proc_cfg_.frm_rate_mul - 1)
                : 0
        );
    }
    if (frame_idx_.load() > total_frames_.load()) {
        total_frames_.store(frame_idx_.load());
    }

    auto process_decoded_frame = [&]() -> int {
        if (enc_cfg_.recalculate_pts) {
            frame->pts = av_rescale_q(
                encoded_frame_idx_, av_inv_q(enc_ctx->framerate), enc_ctx->time_base
            );
        }

        if (resume_session_ != nullptr &&
            source_frame_idx_ < resume_session_->completed_source_frames()) {
            std::vector<AVFrame*> recovered_frames;
            std::string error;
            int frame_ret =
                resume_session_->load_unit(source_frame_idx_, recovered_frames, error);
            if (frame_ret < 0) {
                logger()->critical("Failed to load checkpoint: {}", error);
                return frame_ret;
            }
            const size_t expected_frames =
                processor->get_processing_mode() == processors::ProcessingMode::Interpolate
                    ? static_cast<size_t>(source_frame_idx_ == 0 ? 1 : proc_cfg_.frm_rate_mul)
                    : 1;
            if (recovered_frames.size() != expected_frames) {
                for (AVFrame* recovered_frame : recovered_frames) {
                    av_frame_free(&recovered_frame);
                }
                logger()->critical("Checkpoint processing unit has an invalid frame count");
                return AVERROR_INVALIDDATA;
            }
            for (AVFrame* recovered_frame : recovered_frames) {
                frame_ret = write_frame(recovered_frame, encoder, true);
                av_frame_free(&recovered_frame);
                if (frame_ret < 0) return frame_ret;
            }
            if (processor->get_processing_mode() == processors::ProcessingMode::Interpolate) {
                prev_frame.reset(av_frame_clone(frame.get()));
                if (prev_frame == nullptr) return AVERROR(ENOMEM);
            }
        } else {
            processed_new_source = true;
            clear_checkpoint_frames();

            AVFrame* proc_frame = nullptr;
            int frame_ret = 0;
            switch (processor->get_processing_mode()) {
                case processors::ProcessingMode::Filter:
                    frame_ret = process_filtering(processor, encoder, frame.get(), proc_frame);
                    break;
                case processors::ProcessingMode::Interpolate:
                    frame_ret = process_interpolation(
                        processor, encoder, prev_frame, frame.get(), proc_frame
                    );
                    break;
                default:
                    logger()->critical("Unknown processing mode");
                    return AVERROR(EINVAL);
            }
            if (frame_ret < 0 && frame_ret != AVERROR(EAGAIN)) {
                clear_checkpoint_frames();
                return frame_ret;
            }
            if (resume_session_ != nullptr) {
                std::string error;
                frame_ret =
                    resume_session_->store_unit(source_frame_idx_, checkpoint_frames_, error);
                if (frame_ret >= 0) {
                    frame_ret = resume_session_->checkpoint(
                        source_frame_idx_ + 1, frame_idx_.load(), false, error
                    );
                }
                clear_checkpoint_frames();
                if (frame_ret < 0) {
                    logger()->critical("Failed to publish checkpoint: {}", error);
                    return frame_ret;
                }
            }
        }

        ++source_frame_idx_;
        av_frame_unref(frame.get());
        logger()->debug("Processed frame {}/{}", frame_idx_.load(), total_frames_.load());
        return 0;
    };

    // Read frames from the input file
    while (state_.load() != VideoProcessorState::Aborted) {
        ret = av_read_frame(ifmt_ctx, packet.get());
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                logger()->debug("Reached end of file");
                ret = avcodec_send_packet(dec_ctx, nullptr);
                if (ret < 0 && ret != AVERROR_EOF) {
                    av_strerror(ret, errbuf, sizeof(errbuf));
                    logger()->critical("Error flushing decoder: {}", errbuf);
                    return ret;
                }
                while (state_.load() != VideoProcessorState::Aborted) {
                    if (state_.load() == VideoProcessorState::Paused) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        continue;
                    }
                    ret = avcodec_receive_frame(dec_ctx, frame.get());
                    if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) break;
                    if (ret < 0) {
                        av_strerror(ret, errbuf, sizeof(errbuf));
                        logger()->critical("Error flushing decoded video frame: {}", errbuf);
                        return ret;
                    }
                    ret = process_decoded_frame();
                    if (ret < 0) return ret;
                }
                break;
            }
            av_strerror(ret, errbuf, sizeof(errbuf));
            logger()->critical("Error reading packet: {}", errbuf);
            return ret;
        }

        if (packet->stream_index == in_vstream_idx) {
            // Send the packet to the decoder for decoding
            ret = avcodec_send_packet(dec_ctx, packet.get());
            if (ret < 0) {
                av_strerror(ret, errbuf, sizeof(errbuf));
                logger()->critical("Error sending packet to decoder: {}", errbuf);
                return ret;
            }

            // Process frames decoded from the packet
            while (state_.load() != VideoProcessorState::Aborted) {
                // Sleep for 100 ms if processing is paused
                if (state_.load() == VideoProcessorState::Paused) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }

                // Receive the decoded frame from the decoder
                ret = avcodec_receive_frame(dec_ctx, frame.get());
                if (ret == AVERROR(EAGAIN)) {
                    // No more frames from this packet
                    break;
                } else if (ret < 0) {
                    av_strerror(ret, errbuf, sizeof(errbuf));
                    logger()->critical("Error decoding video frame: {}", errbuf);
                    return ret;
                }

                ret = process_decoded_frame();
                if (ret < 0) return ret;
            }
        } else if ((enc_cfg_.copy_audio_streams || enc_cfg_.copy_subtitle_streams) &&
                   stream_map[packet->stream_index] >= 0) {
            ret = write_raw_packet(packet.get(), ifmt_ctx, ofmt_ctx, stream_map);
            if (ret < 0) {
                return ret;
            }
        }
        av_packet_unref(packet.get());
    }

    if (state_.load() == VideoProcessorState::Aborted) {
        if (resume_session_ != nullptr) {
            std::string error;
            ret = resume_session_->checkpoint(
                source_frame_idx_, frame_idx_.load(), true, error
            );
            if (ret < 0) {
                logger()->critical("Failed to publish cancellation checkpoint: {}", error);
                return ret;
            }
        }
        return AVERROR_EXIT;
    }
    if (resume_session_ != nullptr) {
        resume_session_->set_total_source_frames(source_frame_idx_);
    }

    // Flush the processor or replay its previously committed tail.
    std::vector<AVFrame*> raw_flushed_frames;
    std::string resume_error;
    if (resume_session_ != nullptr && !processed_new_source &&
        resume_session_->completed_source_frames() > 0) {
        ret = resume_session_->load_tail(raw_flushed_frames, resume_error);
    } else {
        clear_checkpoint_frames();
        ret = processor->flush(raw_flushed_frames);
    }
    if (ret < 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        logger()->critical(
            "Error flushing processor: {}{}",
            errbuf,
            resume_error.empty() ? "" : " (" + resume_error + ")"
        );
        return ret;
    }

    // Wrap flushed frames in unique_ptrs
    std::vector<std::unique_ptr<AVFrame, decltype(&avutils::av_frame_deleter)>> flushed_frames;
    for (AVFrame* raw_frame : raw_flushed_frames) {
        flushed_frames.emplace_back(raw_frame, &avutils::av_frame_deleter);
    }

    // Encode and write all flushed frames
    for (auto& flushed_frame : flushed_frames) {
        ret = write_frame(
            flushed_frame.get(),
            encoder,
            resume_session_ != nullptr && !processed_new_source
        );
        if (ret < 0) {
            clear_checkpoint_frames();
            return ret;
        }
    }

    if (resume_session_ != nullptr) {
        if (processed_new_source) {
            ret = resume_session_->store_tail(checkpoint_frames_, resume_error);
        }
        if (ret >= 0) {
            ret = resume_session_->checkpoint(
                source_frame_idx_, frame_idx_.load(), true, resume_error
            );
        }
        clear_checkpoint_frames();
        if (ret < 0) {
            logger()->critical("Failed to publish final checkpoint: {}", resume_error);
            return ret;
        }
    }

    // Flush the encoder
    ret = encoder.flush();
    if (ret < 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        logger()->critical("Error flushing encoder: {}", errbuf);
        return ret;
    }

    return ret;
}

int VideoProcessor::write_frame(AVFrame* frame, encoder::Encoder& encoder, bool recovered) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    int ret = 0;

    if (resume_session_ != nullptr && !recovered) {
        AVFrame* checkpoint_frame = av_frame_clone(frame);
        if (checkpoint_frame == nullptr) {
            return AVERROR(ENOMEM);
        }
        checkpoint_frames_.push_back(checkpoint_frame);
    }

    if (!benchmark_) {
        ret = encoder.write_frame(frame, encoded_frame_idx_);
        if (ret < 0) {
            av_strerror(ret, errbuf, sizeof(errbuf));
            logger()->critical("Error encoding/writing frame: {}", errbuf);
        }
    }
    if (ret >= 0) {
        ++encoded_frame_idx_;
        if (!recovered) {
            int64_t processed = frame_idx_.fetch_add(1) + 1;
            if (processed > total_frames_.load()) {
                total_frames_.store(processed);
            }
        }
    }
    return ret;
}

int VideoProcessor::write_raw_packet(
    AVPacket* packet,
    AVFormatContext* ifmt_ctx,
    AVFormatContext* ofmt_ctx,
    int* stream_map
) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    int ret = 0;

    AVStream* in_stream = ifmt_ctx->streams[packet->stream_index];
    int out_stream_idx = stream_map[packet->stream_index];
    AVStream* out_stream = ofmt_ctx->streams[out_stream_idx];

    av_packet_rescale_ts(packet, in_stream->time_base, out_stream->time_base);
    packet->stream_index = out_stream_idx;

    ret = av_interleaved_write_frame(ofmt_ctx, packet);
    if (ret < 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        logger()->critical("Error muxing audio/subtitle packet: {}", errbuf);
    }
    return ret;
}

int VideoProcessor::process_filtering(
    std::unique_ptr<processors::Processor>& processor,
    encoder::Encoder& encoder,
    AVFrame* frame,
    AVFrame* proc_frame
) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    int ret = 0;

    // Cast the processor to a Filter
    processors::Filter* filter = static_cast<processors::Filter*>(processor.get());

    // Process the frame using the filter
    ret = filter->filter(frame, &proc_frame);

    // Write the processed frame
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        logger()->critical("Error filtering frame: {}", errbuf);
    } else if (ret == 0 && proc_frame != nullptr) {
        auto processed_frame = std::unique_ptr<AVFrame, decltype(&avutils::av_frame_deleter)>(
            proc_frame, &avutils::av_frame_deleter
        );
        ret = write_frame(processed_frame.get(), encoder);
    }
    return ret;
}

int VideoProcessor::process_interpolation(
    std::unique_ptr<processors::Processor>& processor,
    encoder::Encoder& encoder,
    std::unique_ptr<AVFrame, decltype(&avutils::av_frame_deleter)>& prev_frame,
    AVFrame* frame,
    AVFrame* proc_frame
) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    int ret = 0;

    // Cast the processor to an Interpolator
    processors::Interpolator* interpolator =
        static_cast<processors::Interpolator*>(processor.get());

    // Calculate the time step for each frame
    float time_step = 1.0f / static_cast<float>(proc_cfg_.frm_rate_mul);
    float current_time_step = time_step;

    // Check if a scene change is detected
    bool skip_frame = false;
    if (proc_cfg_.scn_det_thresh < 100.0 && prev_frame.get() != nullptr) {
        float frame_diff = avutils::get_frame_diff(prev_frame.get(), frame);
        if (frame_diff > proc_cfg_.scn_det_thresh) {
            logger()->debug(
                "Scene change detected ({:.2f}%), skipping frame {}", frame_diff, frame_idx_.load()
            );
            skip_frame = true;
        }
    }

    // Write the interpolated frames
    for (int i = 0; i < proc_cfg_.frm_rate_mul - 1; i++) {
        // Skip interpolation if this is the first frame
        if (prev_frame == nullptr) {
            break;
        }

        // Get the interpolated frame from the interpolator
        if (!skip_frame) {
            ret =
                interpolator->interpolate(prev_frame.get(), frame, &proc_frame, current_time_step);
        } else {
            ret = 0;
            proc_frame = av_frame_clone(prev_frame.get());
        }

        // Write the interpolated frame
        if (ret < 0 && ret != AVERROR(EAGAIN)) {
            av_strerror(ret, errbuf, sizeof(errbuf));
            logger()->critical("Error interpolating frame: {}", errbuf);
            return ret;
        } else if (ret == 0 && proc_frame != nullptr) {
            auto processed_frame = std::unique_ptr<AVFrame, decltype(&avutils::av_frame_deleter)>(
                proc_frame, &avutils::av_frame_deleter
            );

            ret = write_frame(processed_frame.get(), encoder);
            if (ret < 0) {
                return ret;
            }
        }

        current_time_step += time_step;
    }

    // Write the original frame
    ret = write_frame(frame, encoder);

    // Update the previous frame with the current frame
    prev_frame.reset(av_frame_clone(frame));
    return ret;
}

}  // namespace video2x
