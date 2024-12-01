#include "frames_processor.h"

extern "C" {
#include <libavutil/avutil.h>
}

#include <spdlog/spdlog.h>

#include "avutils.h"

// Deleter for AVFrame unique_ptr
auto av_frame_deleter = [](AVFrame *frame) {
    if (frame != nullptr) {
        av_frame_free(&frame);
        frame = nullptr;
    }
};

// Deleter for AVPacket unique_ptr
auto av_packet_deleter = [](AVPacket *packet) {
    if (packet != nullptr) {
        av_packet_unref(packet);
        av_packet_free(&packet);
        packet = nullptr;
    }
};

// Sets the total number of frames to process in the VideoProcessingContext
void set_total_frames(
    const ProcessorConfig *processor_config,
    VideoProcessingContext *proc_ctx,
    AVFormatContext *ifmt_ctx,
    int in_vstream_idx,
    Processor *processor
) {
    spdlog::debug("Estimating the total number of frames to process");
    proc_ctx->total_frames = get_video_frame_count(ifmt_ctx, in_vstream_idx);

    if (proc_ctx->total_frames <= 0) {
        spdlog::warn("Unable to determine the total number of frames");
        proc_ctx->total_frames = 0;
    } else {
        spdlog::debug("{} frames to process", proc_ctx->total_frames);
    }

    // Set total frames for interpolation
    if (processor->get_processing_mode() == PROCESSING_MODE_INTERPOLATE) {
        proc_ctx->total_frames *= processor_config->frm_rate_mul;
    }
}

int write_frame(
    AVFrame *frame,
    VideoProcessingContext *proc_ctx,
    Encoder &encoder,
    bool benchmark
) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    int ret = 0;

    if (!benchmark) {
        // Set the frame type to none to let the encoder decide
        frame->pict_type = AV_PICTURE_TYPE_NONE;
        ret = encoder.write_frame(frame, proc_ctx->processed_frames);
        if (ret < 0) {
            av_strerror(ret, errbuf, sizeof(errbuf));
            spdlog::critical("Error encoding/writing frame: {}", errbuf);
        }
    }
    return ret;
}

int write_raw_packet(
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

int process_filtering(
    Processor *processor,
    VideoProcessingContext *proc_ctx,
    Encoder &encoder,
    bool benchmark,
    AVFrame *frame,
    AVFrame *raw_processed_frame
) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    int ret = 0;

    // Cast the processor to a Filter
    Filter *filter = static_cast<Filter *>(processor);

    // Process the frame using the filter
    ret = filter->filter(frame, &raw_processed_frame);

    // Write the processed frame
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        spdlog::critical("Error filtering frame: {}", errbuf);
    } else if (ret == 0 && raw_processed_frame != nullptr) {
        auto processed_frame = std::unique_ptr<AVFrame, decltype(av_frame_deleter)>(
            raw_processed_frame, av_frame_deleter
        );
        ret = write_frame(processed_frame.get(), proc_ctx, encoder, benchmark);
    }
    return ret;
}

int process_interpolation(
    Processor *processor,
    const ProcessorConfig *processor_config,
    VideoProcessingContext *proc_ctx,
    Encoder &encoder,
    bool benchmark,
    std::unique_ptr<AVFrame, decltype(av_frame_deleter)> &prev_frame,
    AVFrame *frame,
    AVFrame *raw_processed_frame
) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    int ret = 0;

    // Cast the processor to an Interpolator
    Interpolator *interpolator = static_cast<Interpolator *>(processor);

    // Calculate the time step for each frame
    float time_step = 1.0f / static_cast<float>(processor_config->frm_rate_mul);
    float current_time_step = time_step;

    // Check if a scene change is detected
    bool skip_frame = false;
    if (prev_frame != nullptr) {
        float frame_diff = get_frame_diff(prev_frame.get(), frame);
        if (frame_diff > processor_config->scn_det_thresh) {
            spdlog::debug(
                "Scene change detected ({:.2f}%), skipping frame {}",
                frame_diff,
                proc_ctx->processed_frames
            );
            skip_frame = true;
        }
    }

    // Write the interpolated frames
    for (int i = 0; i < processor_config->frm_rate_mul - 1; i++) {
        // Skip interpolation if this is the first frame
        if (prev_frame == nullptr) {
            break;
        }

        // Get the interpolated frame from the interpolator
        if (!skip_frame) {
            ret = interpolator->interpolate(
                prev_frame.get(), frame, &raw_processed_frame, current_time_step
            );
        } else {
            ret = 0;
            raw_processed_frame = av_frame_clone(prev_frame.get());
        }

        // Write the interpolated frame
        if (ret < 0 && ret != AVERROR(EAGAIN)) {
            av_strerror(ret, errbuf, sizeof(errbuf));
            spdlog::critical("Error interpolating frame: {}", errbuf);
            return ret;
        } else if (ret == 0 && raw_processed_frame != nullptr) {
            auto processed_frame = std::unique_ptr<AVFrame, decltype(av_frame_deleter)>(
                raw_processed_frame, av_frame_deleter
            );

            processed_frame->pts = proc_ctx->processed_frames;
            ret = write_frame(processed_frame.get(), proc_ctx, encoder, benchmark);
            if (ret < 0) {
                return ret;
            }
        }
        proc_ctx->processed_frames++;
        current_time_step += time_step;
    }

    // Write the original frame
    frame->pts = proc_ctx->processed_frames;
    ret = write_frame(frame, proc_ctx, encoder, benchmark);

    // Update the previous frame with the current frame
    prev_frame.reset(av_frame_clone(frame));
    return ret;
}

// Process frames using the selected filter.
int process_frames(
    const EncoderConfig *encoder_config,
    const ProcessorConfig *processor_config,
    VideoProcessingContext *proc_ctx,
    Decoder &decoder,
    Encoder &encoder,
    Processor *processor,
    bool benchmark
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
    std::unique_ptr<AVFrame, decltype(av_frame_deleter)> prev_frame(nullptr, av_frame_deleter);

    // Allocate space for the decoded frames
    std::unique_ptr<AVFrame, decltype(av_frame_deleter)> frame(av_frame_alloc(), av_frame_deleter);
    if (frame == nullptr) {
        spdlog::critical("Error allocating frame");
        return AVERROR(ENOMEM);
    }

    // Allocate space for the decoded packets
    std::unique_ptr<AVPacket, decltype(av_packet_deleter)> packet(
        av_packet_alloc(), av_packet_deleter
    );
    if (packet == nullptr) {
        spdlog::critical("Error allocating packet");
        return AVERROR(ENOMEM);
    }

    // Set the total number of frames in the VideoProcessingContext
    set_total_frames(processor_config, proc_ctx, ifmt_ctx, in_vstream_idx, processor);

    // Read frames from the input file
    while (!proc_ctx->abort) {
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
            while (!proc_ctx->abort) {
                // Sleep for 100 ms if processing is paused
                if (proc_ctx->pause) {
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

                AVFrame *raw_processed_frame = nullptr;

                // Process the frame based on the selected processing mode
                switch (processor->get_processing_mode()) {
                    case PROCESSING_MODE_FILTER: {
                        ret = process_filtering(
                            processor,
                            proc_ctx,
                            encoder,
                            benchmark,
                            frame.get(),
                            raw_processed_frame
                        );
                        break;
                    }
                    case PROCESSING_MODE_INTERPOLATE: {
                        ret = process_interpolation(
                            processor,
                            processor_config,
                            proc_ctx,
                            encoder,
                            benchmark,
                            prev_frame,
                            frame.get(),
                            raw_processed_frame
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
                proc_ctx->processed_frames++;
                spdlog::debug(
                    "Processed frame {}/{}", proc_ctx->processed_frames, proc_ctx->total_frames
                );
            }
        } else if (encoder_config->copy_streams && stream_map[packet->stream_index] >= 0) {
            write_raw_packet(packet.get(), ifmt_ctx, ofmt_ctx, stream_map);
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
    std::vector<std::unique_ptr<AVFrame, decltype(av_frame_deleter)>> flushed_frames;
    for (AVFrame *raw_frame : raw_flushed_frames) {
        flushed_frames.emplace_back(raw_frame, av_frame_deleter);
    }

    // Encode and write all flushed frames
    for (auto &flushed_frame : flushed_frames) {
        ret = write_frame(flushed_frame.get(), proc_ctx, encoder, benchmark);
        if (ret < 0) {
            return ret;
        }
        proc_ctx->processed_frames++;
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
