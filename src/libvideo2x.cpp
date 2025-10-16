#include "libvideo2x.h"
#include <libavcodec/avcodec.h>

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

#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>
#include <future>
#include <atomic>

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
    int ret = 0;

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

    // Create and initialize the appropriate filter (one per batch element)
    std::vector<std::unique_ptr<processors::Processor>> processor_instances;
    processor_instances.reserve(proc_cfg_.batch_size);
    for (size_t i = 0; i < proc_cfg_.batch_size; ++i) {
        processor_instances.emplace_back(
            processors::ProcessorFactory::instance().create_processor(proc_cfg_, vk_device_idx_)
        );
        if (processor_instances[i] == nullptr) {
            return handle_error(-1, "Failed to create filter instance");
        }
    }
    if (processor_instances.empty()) {
        return handle_error(-1, "No filter instances created");
    }

    // Initialize output dimensions based on filter configuration
    int output_width = 0, output_height = 0;
    processor_instances[0]->get_output_dimensions(
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
    for (auto& processor : processor_instances) {
        ret = processor->init(dec_ctx, encoder.get_encoder_context(), hw_ctx.get());
        if (ret < 0) {
            return handle_error(ret, "Failed to initialize filter");
        }
    }
    logger()->debug("Initialized {} filter instances", processor_instances.size());

    // Process frames using the encoder and decoder
    ret = process_frames(decoder, encoder, processor_instances);
    if (ret < 0) {
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
    state_.store(VideoProcessorState::Completed);
    return 0;
}

// Process frames using the selected filter.
int VideoProcessor::process_frames(
    decoder::Decoder& decoder,
    encoder::Encoder& encoder,
    std::vector<std::unique_ptr<processors::Processor>>& processor_instances
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

    // Mode specific initialization
    switch (processor_instances[0]->get_processing_mode()) {                    
        case processors::ProcessingMode::Filter: {
            logger()->info("filtering using a batch size of {}", proc_cfg_.batch_size);
            break;
        }
        case processors::ProcessingMode::Interpolate: {
            // Set total frames for interpolation
            total_frames_.store(total_frames_.load() * proc_cfg_.frm_rate_mul);
            break;
        }
    }

    // Read frames from the input file
    std::vector<std::unique_ptr<AVFrame, decltype(&avutils::av_frame_deleter)>> batch_frames;
    std::vector<int64_t> batched_pts;
    batch_frames.reserve(std::size_t(proc_cfg_.batch_size));
    batched_pts.reserve(std::size_t(proc_cfg_.batch_size));
    while (state_.load() != VideoProcessorState::Aborted) {
        ret = av_read_frame(ifmt_ctx, packet.get());
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                logger()->debug("Reached end of file");
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

                // Process the frame based on the selected processing mode
                AVFrame* proc_frame = nullptr;
                switch (processor_instances[0]->get_processing_mode()) {                    
                    case processors::ProcessingMode::Filter: {
                        //logger()->info("Processing frame {} in filter mode", frame_idx_.load() + batch_frames.size());
                        // Calculate this frame's presentation timestamp (PTS)
                        frame->pts = av_rescale_q(frame_idx_ + batch_frames.size(), av_inv_q(enc_ctx->framerate), enc_ctx->time_base);
                        
                        // Clone decoded frame and add to batch
                        AVFrame* cloned = av_frame_clone(frame.get());
                        if (!cloned) { logger()->critical("Frame clone failed"); return AVERROR(ENOMEM);}
                        batch_frames.emplace_back(cloned, &avutils::av_frame_deleter);
                        batched_pts.push_back(frame->pts);
                        av_frame_unref(frame.get());

                        if (batch_frames.size() == proc_cfg_.batch_size) {
                            ret = process_batch_filtering(processor_instances, encoder, batch_frames, batched_pts);
                            if (ret < 0) return ret;
                            batch_frames.clear();
                            batched_pts.clear();
                        }
                        break;
                    }
                    case processors::ProcessingMode::Interpolate: {
                        // Calculate this frame's presentation timestamp (PTS)
                        frame->pts = av_rescale_q(frame_idx_, av_inv_q(enc_ctx->framerate), enc_ctx->time_base);

                        ret = process_interpolation(
                            processor_instances[0], encoder, prev_frame, frame.get(), proc_frame
                        );
                        frame_idx_.fetch_add(1);
                        break;
                    }
                    default:
                        logger()->critical("Unknown processing mode");
                        return -1;
                }
                if (ret < 0 && ret != AVERROR(EAGAIN)) {
                    return ret;
                }
                av_frame_unref(frame.get());
                logger()->debug("Processed frame {}/{}", frame_idx_.load(), total_frames_.load());
            }
        } else if (enc_cfg_.copy_streams && stream_map[packet->stream_index] >= 0) {
            ret = write_raw_packet(packet.get(), ifmt_ctx, ofmt_ctx, stream_map);
            if (ret < 0) {
                return ret;
            }
        }
        av_packet_unref(packet.get());
    }

    // Flush batch processing for filtering mode
    if (processor_instances[0]->get_processing_mode() == processors::ProcessingMode::Filter &&
    !batch_frames.empty()) {
        int filteringFlushRet = process_batch_filtering(processor_instances, encoder, batch_frames, batched_pts);
        if (filteringFlushRet < 0) return filteringFlushRet;
        batch_frames.clear();
        batched_pts.clear();
    }

    // Flush the processor
    for (auto& processor : processor_instances) {
        std::vector<AVFrame*> raw_flushed_frames;
        ret = processor->flush(raw_flushed_frames);
        if (ret < 0) {
            av_strerror(ret, errbuf, sizeof(errbuf));
            logger()->critical("Error flushing processor: {}", errbuf);
            return ret;
        }
    
        // Wrap flushed frames in unique_ptrs
        std::vector<std::unique_ptr<AVFrame, decltype(&avutils::av_frame_deleter)>> flushed_frames;
        for (AVFrame* raw_frame : raw_flushed_frames) {
            flushed_frames.emplace_back(raw_frame, &avutils::av_frame_deleter);
        }
    
        // Encode and write all flushed frames
        for (auto& flushed_frame : flushed_frames) {
            ret = write_frame(flushed_frame.get(), encoder);
            if (ret < 0) {
                return ret;
            }
            frame_idx_.fetch_add(1);
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

// Helper: Parallel filtering, ordered emission
int VideoProcessor::process_batch_filtering(
    std::vector<std::unique_ptr<processors::Processor>>& processor_instances,
    encoder::Encoder& encoder,
    std::vector<std::unique_ptr<AVFrame, decltype(&avutils::av_frame_deleter)>>& batch_frames,
    std::vector<int64_t>& batched_pts
) {
    logger()->info("executing parallel batch of size {}", batch_frames.size());

    using UPtrAVFrame = std::unique_ptr<AVFrame, decltype(&avutils::av_frame_deleter)>;
    std::vector<UPtrAVFrame> processed_frames;
    processed_frames.reserve(batch_frames.size());
    for (size_t i = 0; i < batch_frames.size(); ++i) {
        processed_frames.emplace_back(nullptr, avutils::av_frame_deleter);
    }
    std::vector<int> results(batch_frames.size(), 0);

    static boost::asio::thread_pool pool(std::thread::hardware_concurrency());

    std::vector<std::shared_ptr<std::promise<void>>> promises(batch_frames.size());
    std::vector<std::future<void>> futures;
    std::atomic<size_t> next_to_write {0};
    std::atomic<int> first_error {0}; 
    for (size_t i = 0; i < batch_frames.size(); ++i) {
        promises[i] = std::make_shared<std::promise<void>>();
        futures.push_back(promises[i]->get_future());
        boost::asio::post(pool, [&, i, promise = promises[i]] {
            AVFrame* filtered = nullptr;
            results[i] = static_cast<processors::Filter*>(processor_instances[i].get())->filter(batch_frames[i].get(), &filtered);

            if (results[i] == 0 && filtered) {
                filtered->pts = batched_pts[i];
                processed_frames[i].reset(filtered);
            }
            // New: Each thread now tries to write, but only in order.
            while (true) {
                // If we hit an earlier error, don't proceed to write
                if (first_error.load() < 0)
                    break;

                if (next_to_write.load() == i) {
                    if (results[i] < 0) {
                        // This thread encountered an error -- record it
                        first_error.store(results[i]);
                        break;
                    }
                    if (processed_frames[i]) {
                        int ret = write_frame(processed_frames[i].get(), encoder);
                        if (ret < 0) {
                            // Write error, record and quit
                            first_error.store(ret);
                            break;
                        }
                        frame_idx_.fetch_add(1);
                    }
                    // Increment so next thread may proceed
                    next_to_write.fetch_add(1);
                    logger()->info("Frame {} processed and written", batched_pts[i]);
                    break;
                } else {
                    // Not this frame's turn yet
                    std::this_thread::yield();
                }
            }
            promise->set_value();
        });
    }

    // Wait for all emissions (or early error bailout)
    for (auto& f : futures) f.get();

    if (first_error.load() < 0) return first_error.load();
    return 0;
}

int VideoProcessor::write_frame(AVFrame* frame, encoder::Encoder& encoder) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    int ret = 0;

    if (!benchmark_) {
        ret = encoder.write_frame(frame, frame_idx_.load());
        if (ret < 0) {
            av_strerror(ret, errbuf, sizeof(errbuf));
            logger()->critical("Error encoding/writing frame: {}", errbuf);
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

/*
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
*/

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

        frame_idx_.fetch_add(1);
        current_time_step += time_step;
    }

    // Write the original frame
    ret = write_frame(frame, encoder);

    // Update the previous frame with the current frame
    prev_frame.reset(av_frame_clone(frame));
    return ret;
}

}  // namespace video2x
