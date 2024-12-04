#include "libvideo2x.h"

extern "C" {
#include <libavutil/avutil.h>
}

#include <spdlog/spdlog.h>

#include "avutils.h"
#include "decoder.h"
#include "encoder.h"
#include "logging.h"
#include "processor.h"
#include "processor_factory.h"

VideoProcessor::VideoProcessor(
    const ProcessorConfig proc_cfg,
    const EncoderConfig enc_cfg,
    const uint32_t vk_device_index,
    const AVHWDeviceType hw_device_type,
    const Video2xLogLevel log_level,
    const bool benchmark
)
    : proc_cfg_(proc_cfg),
      enc_cfg_(enc_cfg),
      vk_device_index_(vk_device_index),
      hw_device_type_(hw_device_type),
      benchmark_(benchmark) {
    set_log_level(log_level);
}

int VideoProcessor::process(
    const std::filesystem::path in_fname,
    const std::filesystem::path out_fname
) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    int ret = 0;

    // Create a smart pointer to manage the hardware device context
    std::unique_ptr<AVBufferRef, decltype(&av_bufferref_deleter)> hw_ctx(
        nullptr, &av_bufferref_deleter
    );

    // Initialize hardware device context
    if (hw_device_type_ != AV_HWDEVICE_TYPE_NONE) {
        AVBufferRef *tmp_hw_ctx = nullptr;
        ret = av_hwdevice_ctx_create(&tmp_hw_ctx, hw_device_type_, NULL, NULL, 0);
        if (ret < 0) {
            av_strerror(ret, errbuf, sizeof(errbuf));
            spdlog::critical("Error initializing hardware device context: {}", errbuf);
            return ret;
        }
        hw_ctx.reset(tmp_hw_ctx);
    }

    // Initialize input decoder
    Decoder decoder;
    ret = decoder.init(hw_device_type_, hw_ctx.get(), in_fname);
    if (ret < 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        spdlog::critical("Failed to initialize decoder: {}", errbuf);
        return ret;
    }

    AVFormatContext *ifmt_ctx = decoder.get_format_context();
    AVCodecContext *dec_ctx = decoder.get_codec_context();
    int in_vstream_idx = decoder.get_video_stream_index();

    // Create and initialize the appropriate filter
    std::unique_ptr<Processor> processor(
        ProcessorFactory::instance().create_processor(proc_cfg_, vk_device_index_)
    );
    if (processor == nullptr) {
        spdlog::critical("Failed to create filter instance");
        return -1;
    }

    // Initialize output dimensions based on filter configuration
    int output_width = 0, output_height = 0;
    processor->get_output_dimensions(
        proc_cfg_, dec_ctx->width, dec_ctx->height, output_width, output_height
    );
    if (output_width <= 0 || output_height <= 0) {
        spdlog::critical("Failed to determine the output dimensions");
        return -1;
    }

    // Update encoder frame rate multiplier
    enc_cfg_.frm_rate_mul = proc_cfg_.frm_rate_mul;

    // Initialize the encoder
    Encoder encoder;
    ret = encoder.init(
        hw_ctx.get(),
        out_fname,
        ifmt_ctx,
        dec_ctx,
        enc_cfg_,
        output_width,
        output_height,
        in_vstream_idx
    );
    if (ret < 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        spdlog::critical("Failed to initialize encoder: {}", errbuf);
        return ret;
    }

    // Initialize the filter
    ret = processor->init(dec_ctx, encoder.get_encoder_context(), hw_ctx.get());
    if (ret < 0) {
        spdlog::critical("Failed to initialize filter");
        return ret;
    }

    // Process frames using the encoder and decoder
    ret = process_frames(decoder, encoder, processor);
    if (ret < 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        spdlog::critical("Error processing frames: {}", errbuf);
        return ret;
    }

    // Write the output file trailer
    av_write_trailer(encoder.get_format_context());

    if (ret < 0 && ret != AVERROR_EOF) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        spdlog::critical("Error occurred: {}", errbuf);
        return ret;
    }
    return 0;
}

// Process frames using the selected filter.
int VideoProcessor::process_frames(
    Decoder &decoder,
    Encoder &encoder,
    std::unique_ptr<Processor> &processor
) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    int ret = 0;

    // Get required objects
    AVFormatContext *ifmt_ctx = decoder.get_format_context();
    AVCodecContext *dec_ctx = decoder.get_codec_context();
    int in_vstream_idx = decoder.get_video_stream_index();
    AVFormatContext *ofmt_ctx = encoder.get_format_context();
    int *stream_map = encoder.get_stream_map();

    // Reference to the previous frame does not require allocation
    // It will be cloned from the current frame
    std::unique_ptr<AVFrame, decltype(&av_frame_deleter)> prev_frame(nullptr, &av_frame_deleter);

    // Allocate space for the decoded frames
    std::unique_ptr<AVFrame, decltype(&av_frame_deleter)> frame(
        av_frame_alloc(), &av_frame_deleter
    );
    if (frame == nullptr) {
        spdlog::critical("Error allocating frame");
        return AVERROR(ENOMEM);
    }

    // Allocate space for the decoded packets
    std::unique_ptr<AVPacket, decltype(&av_packet_deleter)> packet(
        av_packet_alloc(), &av_packet_deleter
    );
    if (packet == nullptr) {
        spdlog::critical("Error allocating packet");
        return AVERROR(ENOMEM);
    }

    // Set the total number of frames in the VideoProcessingContext
    spdlog::debug("Estimating the total number of frames to process");
    total_frames_ = get_video_frame_count(ifmt_ctx, in_vstream_idx);

    if (total_frames_ <= 0) {
        spdlog::warn("Unable to determine the total number of frames");
        total_frames_ = 0;
    } else {
        spdlog::debug("{} frames to process", total_frames_.load());
    }

    // Set total frames for interpolation
    if (processor->get_processing_mode() == ProcessingMode::Interpolate) {
        total_frames_.store(total_frames_.load() * proc_cfg_.frm_rate_mul);
    }

    // Read frames from the input file
    while (!aborted_.load()) {
        ret = av_read_frame(ifmt_ctx, packet.get());
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                spdlog::debug("Reached end of file");
                break;
            }
            av_strerror(ret, errbuf, sizeof(errbuf));
            spdlog::critical("Error reading packet: {}", errbuf);
            return ret;
        }

        if (packet->stream_index == in_vstream_idx) {
            // Send the packet to the decoder for decoding
            ret = avcodec_send_packet(dec_ctx, packet.get());
            if (ret < 0) {
                av_strerror(ret, errbuf, sizeof(errbuf));
                spdlog::critical("Error sending packet to decoder: {}", errbuf);
                return ret;
            }

            // Process frames decoded from the packet
            while (!aborted_.load()) {
                // Sleep for 100 ms if processing is paused
                if (paused_.load()) {
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
                    spdlog::critical("Error decoding video frame: {}", errbuf);
                    return ret;
                }

                // Process the frame based on the selected processing mode
                AVFrame *proc_frame = nullptr;
                switch (processor->get_processing_mode()) {
                    case ProcessingMode::Filter: {
                        ret = process_filtering(processor, encoder, frame.get(), proc_frame);
                        break;
                    }
                    case ProcessingMode::Interpolate: {
                        ret = process_interpolation(
                            processor, encoder, prev_frame, frame.get(), proc_frame
                        );
                        break;
                    }
                    default:
                        spdlog::critical("Unknown processing mode");
                        return -1;
                }
                if (ret < 0 && ret != AVERROR(EAGAIN)) {
                    return ret;
                }
                av_frame_unref(frame.get());
                frame_index_++;
                spdlog::debug("Processed frame {}/{}", frame_index_.load(), total_frames_.load());
            }
        } else if (enc_cfg_.copy_streams && stream_map[packet->stream_index] >= 0) {
            ret = write_raw_packet(packet.get(), ifmt_ctx, ofmt_ctx, stream_map);
            if (ret < 0) {
                return ret;
            }
        }
        av_packet_unref(packet.get());
    }

    // Flush the filter
    std::vector<AVFrame *> raw_flushed_frames;
    ret = processor->flush(raw_flushed_frames);
    if (ret < 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        spdlog::critical("Error flushing filter: {}", errbuf);
        return ret;
    }

    // Wrap flushed frames in unique_ptrs
    std::vector<std::unique_ptr<AVFrame, decltype(&av_frame_deleter)>> flushed_frames;
    for (AVFrame *raw_frame : raw_flushed_frames) {
        flushed_frames.emplace_back(raw_frame, &av_frame_deleter);
    }

    // Encode and write all flushed frames
    for (auto &flushed_frame : flushed_frames) {
        ret = write_frame(flushed_frame.get(), encoder);
        if (ret < 0) {
            return ret;
        }
        frame_index_++;
    }

    // Flush the encoder
    ret = encoder.flush();
    if (ret < 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        spdlog::critical("Error flushing encoder: {}", errbuf);
        return ret;
    }

    return ret;
}

int VideoProcessor::write_frame(AVFrame *frame, Encoder &encoder) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    int ret = 0;

    if (!benchmark_) {
        // Set the frame type to none to let the encoder decide
        frame->pict_type = AV_PICTURE_TYPE_NONE;
        ret = encoder.write_frame(frame, frame_index_);
        if (ret < 0) {
            av_strerror(ret, errbuf, sizeof(errbuf));
            spdlog::critical("Error encoding/writing frame: {}", errbuf);
        }
    }
    return ret;
}

int VideoProcessor::write_raw_packet(
    AVPacket *packet,
    AVFormatContext *ifmt_ctx,
    AVFormatContext *ofmt_ctx,
    int *stream_map
) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    int ret = 0;

    AVStream *in_stream = ifmt_ctx->streams[packet->stream_index];
    int out_stream_index = stream_map[packet->stream_index];
    AVStream *out_stream = ofmt_ctx->streams[out_stream_index];

    av_packet_rescale_ts(packet, in_stream->time_base, out_stream->time_base);
    packet->stream_index = out_stream_index;

    ret = av_interleaved_write_frame(ofmt_ctx, packet);
    if (ret < 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        spdlog::critical("Error muxing audio/subtitle packet: {}", errbuf);
    }
    return ret;
}

int VideoProcessor::process_filtering(
    std::unique_ptr<Processor> &processor,
    Encoder &encoder,
    AVFrame *frame,
    AVFrame *proc_frame
) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    int ret = 0;

    // Cast the processor to a Filter
    Filter *filter = static_cast<Filter *>(processor.get());

    // Process the frame using the filter
    ret = filter->filter(frame, &proc_frame);

    // Write the processed frame
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        spdlog::critical("Error filtering frame: {}", errbuf);
    } else if (ret == 0 && proc_frame != nullptr) {
        auto processed_frame =
            std::unique_ptr<AVFrame, decltype(&av_frame_deleter)>(proc_frame, &av_frame_deleter);
        ret = write_frame(processed_frame.get(), encoder);
    }
    return ret;
}

int VideoProcessor::process_interpolation(
    std::unique_ptr<Processor> &processor,
    Encoder &encoder,
    std::unique_ptr<AVFrame, decltype(&av_frame_deleter)> &prev_frame,
    AVFrame *frame,
    AVFrame *proc_frame
) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    int ret = 0;

    // Cast the processor to an Interpolator
    Interpolator *interpolator = static_cast<Interpolator *>(processor.get());

    // Calculate the time step for each frame
    float time_step = 1.0f / static_cast<float>(proc_cfg_.frm_rate_mul);
    float current_time_step = time_step;

    // Check if a scene change is detected
    bool skip_frame = false;
    if (prev_frame.get() != nullptr) {
        float frame_diff = get_frame_diff(prev_frame.get(), frame);
        if (frame_diff > proc_cfg_.scn_det_thresh) {
            spdlog::debug(
                "Scene change detected ({:.2f}%), skipping frame {}",
                frame_diff,
                frame_index_.load()
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
            spdlog::critical("Error interpolating frame: {}", errbuf);
            return ret;
        } else if (ret == 0 && proc_frame != nullptr) {
            auto processed_frame = std::unique_ptr<AVFrame, decltype(&av_frame_deleter)>(
                proc_frame, &av_frame_deleter
            );

            processed_frame->pts = frame_index_;
            ret = write_frame(processed_frame.get(), encoder);
            if (ret < 0) {
                return ret;
            }
        }

        frame_index_++;
        current_time_step += time_step;
    }

    // Write the original frame
    frame->pts = frame_index_;
    ret = write_frame(frame, encoder);

    // Update the previous frame with the current frame
    prev_frame.reset(av_frame_clone(frame));
    return ret;
}
