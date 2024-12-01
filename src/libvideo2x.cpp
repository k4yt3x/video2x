#include "libvideo2x.h"

#include <libavutil/frame.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>

extern "C" {
#include <libavutil/avutil.h>
}

#include <spdlog/spdlog.h>

#include "avutils.h"
#include "decoder.h"
#include "encoder.h"
#include "filter_libplacebo.h"
#include "filter_realesrgan.h"
#include "interpolator_rife.h"
#include "processor.h"

// Process frames using the selected filter.
static int process_frames(
    const EncoderConfig *encoder_config,
    const ProcessorConfig *processor_config,
    VideoProcessingContext *proc_ctx,
    Decoder &decoder,
    Encoder &encoder,
    Processor *processor,
    bool benchmark = false
) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    int ret = 0;

    // Get required objects
    AVFormatContext *ifmt_ctx = decoder.get_format_context();
    AVCodecContext *dec_ctx = decoder.get_codec_context();
    int in_vstream_idx = decoder.get_video_stream_index();
    AVFormatContext *ofmt_ctx = encoder.get_format_context();
    int *stream_map = encoder.get_stream_map();

    // Get total number of frames
    spdlog::debug("Reading total number of frames");
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

    // Allocate AVFrame for the current processed frame
    auto av_frame_deleter = [](AVFrame *frame) { av_frame_free(&frame); };
    std::unique_ptr<AVFrame, decltype(av_frame_deleter)> frame(av_frame_alloc(), av_frame_deleter);
    if (!frame) {
        ret = AVERROR(ENOMEM);
        return ret;
    }

    // Allocate AVFrame for the previous processed frame
    auto av_frame_deleter_prev = [](AVFrame *prev_frame) {
        if (prev_frame != nullptr) {
            av_frame_unref(prev_frame);
            prev_frame = nullptr;
        }
    };
    std::unique_ptr<AVFrame, decltype(av_frame_deleter_prev)> prev_frame(
        nullptr, av_frame_deleter_prev
    );

    // Allocate AVPacket for reading frames
    auto av_packet_deleter = [](AVPacket *packet) {
        av_packet_unref(packet);
        av_packet_free(&packet);
    };
    std::unique_ptr<AVPacket, decltype(av_packet_deleter)> packet(
        av_packet_alloc(), av_packet_deleter
    );
    if (!packet) {
        spdlog::critical("Could not allocate AVPacket");
        return AVERROR(ENOMEM);
    }

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
            ret = avcodec_send_packet(dec_ctx, packet.get());
            if (ret < 0) {
                av_strerror(ret, errbuf, sizeof(errbuf));
                spdlog::critical("Error sending packet to decoder: {}", errbuf);
                return ret;
            }

            while (!proc_ctx->abort) {
                if (proc_ctx->pause) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }

                ret = avcodec_receive_frame(dec_ctx, frame.get());
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    spdlog::debug("Frame not ready");
                    break;
                } else if (ret < 0) {
                    av_strerror(ret, errbuf, sizeof(errbuf));
                    spdlog::critical("Error decoding video frame: {}", errbuf);
                    return ret;
                }

                AVFrame *raw_processed_frame = nullptr;

                switch (processor->get_processing_mode()) {
                    case PROCESSING_MODE_FILTER: {
                        Filter *filter = dynamic_cast<Filter *>(processor);
                        ret = filter->filter(frame.get(), &raw_processed_frame);
                        if (ret < 0 && ret != AVERROR(EAGAIN)) {
                            av_strerror(ret, errbuf, sizeof(errbuf));
                            return ret;
                        } else if (ret == 0 && raw_processed_frame != nullptr) {
                            auto processed_frame =
                                std::unique_ptr<AVFrame, decltype(av_frame_deleter)>(
                                    raw_processed_frame, av_frame_deleter
                                );

                            if (!benchmark) {
                                ret = encoder.write_frame(
                                    processed_frame.get(), proc_ctx->processed_frames
                                );
                                if (ret < 0) {
                                    av_strerror(ret, errbuf, sizeof(errbuf));
                                    spdlog::critical("Error encoding/writing frame: {}", errbuf);
                                    return ret;
                                }
                            }
                        }
                        break;
                    }
                    case PROCESSING_MODE_INTERPOLATE: {
                        Interpolator *interpolator = dynamic_cast<Interpolator *>(processor);

                        float time_step = 1.0f / static_cast<float>(processor_config->frm_rate_mul);
                        float current_time_step = time_step;

                        bool skip_frame = false;
                        if (prev_frame != nullptr) {
                            float frame_diff = get_frame_diff(prev_frame.get(), frame.get());
                            if (frame_diff > processor_config->scn_det_thresh) {
                                spdlog::debug(
                                    "Scene change detected ({:.2f}%), skipping frame {}",
                                    frame_diff,
                                    proc_ctx->processed_frames
                                );
                                skip_frame = true;
                            }
                        }

                        while (prev_frame != nullptr && current_time_step < 1.0f) {
                            if (!skip_frame) {
                                ret = interpolator->interpolate(
                                    prev_frame.get(),
                                    frame.get(),
                                    &raw_processed_frame,
                                    current_time_step
                                );
                            } else {
                                ret = 0;
                                raw_processed_frame = av_frame_clone(prev_frame.get());
                            }

                            if (ret < 0 && ret != AVERROR(EAGAIN)) {
                                av_strerror(ret, errbuf, sizeof(errbuf));
                                return ret;
                            } else if (ret == 0 && raw_processed_frame != nullptr) {
                                auto processed_frame =
                                    std::unique_ptr<AVFrame, decltype(av_frame_deleter)>(
                                        raw_processed_frame, av_frame_deleter
                                    );

                                if (!benchmark) {
                                    processed_frame->pts = proc_ctx->processed_frames;
                                    ret = encoder.write_frame(
                                        processed_frame.get(), proc_ctx->processed_frames
                                    );
                                    if (ret < 0) {
                                        av_strerror(ret, errbuf, sizeof(errbuf));
                                        spdlog::critical(
                                            "Error encoding/writing frame: {}", errbuf
                                        );
                                        return ret;
                                    }
                                }
                            }
                            proc_ctx->processed_frames++;
                            current_time_step += time_step;
                        }

                        if (!benchmark) {
                            frame->pts = proc_ctx->processed_frames;
                            ret = encoder.write_frame(frame.get(), proc_ctx->processed_frames);
                            if (ret < 0) {
                                av_strerror(ret, errbuf, sizeof(errbuf));
                                spdlog::critical("Error encoding/writing frame: {}", errbuf);
                                return ret;
                            }
                        }
                        prev_frame.reset(av_frame_clone(frame.get()));
                        break;
                    }
                    default:
                        spdlog::critical("Unknown processing mode");
                        return -1;
                }
                av_frame_unref(frame.get());
                proc_ctx->processed_frames++;
                spdlog::debug(
                    "Processed frame {}/{}", proc_ctx->processed_frames, proc_ctx->total_frames
                );
            }
        } else if (encoder_config->copy_streams && stream_map[packet->stream_index] >= 0) {
            AVStream *in_stream = ifmt_ctx->streams[packet->stream_index];
            int out_stream_index = stream_map[packet->stream_index];
            AVStream *out_stream = ofmt_ctx->streams[out_stream_index];

            av_packet_rescale_ts(packet.get(), in_stream->time_base, out_stream->time_base);
            packet->stream_index = out_stream_index;

            ret = av_interleaved_write_frame(ofmt_ctx, packet.get());
            if (ret < 0) {
                av_strerror(ret, errbuf, sizeof(errbuf));
                spdlog::critical("Error muxing audio/subtitle packet: {}", errbuf);
                return ret;
            }
        }
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
        ret = encoder.write_frame(flushed_frame.get(), proc_ctx->processed_frames);
        if (ret < 0) {
            av_strerror(ret, errbuf, sizeof(errbuf));
            spdlog::critical("Error encoding/writing flushed frame: {}", errbuf);
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

extern "C" int process_video(
    const CharType *in_fname,
    const CharType *out_fname,
    Libvideo2xLogLevel log_level,
    bool benchmark,
    uint32_t vk_device_index,
    AVHWDeviceType hw_type,
    const ProcessorConfig *processor_config,
    EncoderConfig *encoder_config,
    VideoProcessingContext *proc_ctx
) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    int ret = 0;

    // Set the log level for FFmpeg and spdlog
    switch (log_level) {
        case LIBVIDEO2X_LOG_LEVEL_TRACE:
            av_log_set_level(AV_LOG_TRACE);
            spdlog::set_level(spdlog::level::trace);
            break;
        case LIBVIDEO2X_LOG_LEVEL_DEBUG:
            av_log_set_level(AV_LOG_DEBUG);
            spdlog::set_level(spdlog::level::debug);
            break;
        case LIBVIDEO2X_LOG_LEVEL_INFO:
            av_log_set_level(AV_LOG_INFO);
            spdlog::set_level(spdlog::level::info);
            break;
        case LIBVIDEO2X_LOG_LEVEL_WARNING:
            av_log_set_level(AV_LOG_WARNING);
            spdlog::set_level(spdlog::level::warn);
            break;
        case LIBVIDEO2X_LOG_LEVEL_ERROR:
            av_log_set_level(AV_LOG_ERROR);
            spdlog::set_level(spdlog::level::err);
            break;
        case LIBVIDEO2X_LOG_LEVEL_CRITICAL:
            av_log_set_level(AV_LOG_FATAL);
            spdlog::set_level(spdlog::level::critical);
            break;
        case LIBVIDEO2X_LOG_LEVEL_OFF:
            av_log_set_level(AV_LOG_QUIET);
            spdlog::set_level(spdlog::level::off);
            break;
        default:
            av_log_set_level(AV_LOG_INFO);
            spdlog::set_level(spdlog::level::info);
            break;
    }

    // Convert the file names to std::filesystem::path
    std::filesystem::path in_fpath(in_fname);
    std::filesystem::path out_fpath(out_fname);

    auto hw_ctx_deleter = [](AVBufferRef *ref) {
        if (ref) {
            av_buffer_unref(&ref);
        }
    };
    std::unique_ptr<AVBufferRef, decltype(hw_ctx_deleter)> hw_ctx(nullptr, hw_ctx_deleter);

    // Initialize hardware device context
    if (hw_type != AV_HWDEVICE_TYPE_NONE) {
        AVBufferRef *tmp_hw_ctx = nullptr;
        ret = av_hwdevice_ctx_create(&tmp_hw_ctx, hw_type, NULL, NULL, 0);
        if (ret < 0) {
            av_strerror(ret, errbuf, sizeof(errbuf));
            spdlog::critical("Error initializing hardware device context: {}", errbuf);
            return ret;
        }
        hw_ctx.reset(tmp_hw_ctx);
    }

    // Initialize input decoder
    Decoder decoder;
    ret = decoder.init(hw_type, hw_ctx.get(), in_fpath);
    if (ret < 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        spdlog::critical("Failed to initialize decoder: {}", errbuf);
        return ret;
    }

    AVFormatContext *ifmt_ctx = decoder.get_format_context();
    AVCodecContext *dec_ctx = decoder.get_codec_context();
    int in_vstream_idx = decoder.get_video_stream_index();

    // Initialize output dimensions based on filter configuration
    int output_width = 0, output_height = 0;
    switch (processor_config->processor_type) {
        case PROCESSOR_LIBPLACEBO:
            output_width = processor_config->width;
            output_height = processor_config->height;
            break;
        case PROCESSOR_REALESRGAN:
            output_width = dec_ctx->width * processor_config->scaling_factor;
            output_height = dec_ctx->height * processor_config->scaling_factor;
            break;
        case PROCESSOR_RIFE:
            output_width = dec_ctx->width;
            output_height = dec_ctx->height;
            break;
        default:
            spdlog::critical("Unknown filter type");
            return -1;
    }
    spdlog::debug("Output video dimensions: {}x{}", output_width, output_height);

    // Update encoder configuration with output dimensions
    encoder_config->width = output_width;
    encoder_config->height = output_height;

    // Initialize the encoder
    Encoder encoder;
    ret = encoder.init(
        hw_ctx.get(), out_fpath, ifmt_ctx, dec_ctx, encoder_config, processor_config, in_vstream_idx
    );
    if (ret < 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        spdlog::critical("Failed to initialize encoder: {}", errbuf);
        return ret;
    }

    // Write the output file header
    ret = avformat_write_header(encoder.get_format_context(), NULL);
    if (ret < 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        spdlog::critical("Error occurred when opening output file: {}", errbuf);
        return ret;
    }

    // Create and initialize the appropriate filter
    std::unique_ptr<Processor> processor;
    switch (processor_config->processor_type) {
        case PROCESSOR_LIBPLACEBO: {
            const auto &config = processor_config->config.libplacebo;
            if (!config.shader_path) {
                spdlog::critical("Shader path must be provided for the libplacebo filter");
                return -1;
            }
            processor = std::make_unique<FilterLibplacebo>(
                vk_device_index,
                std::filesystem::path(config.shader_path),
                processor_config->width,
                processor_config->height
            );
            break;
        }
        case PROCESSOR_REALESRGAN: {
            const auto &config = processor_config->config.realesrgan;
            if (!config.model_name) {
                spdlog::critical("Model name must be provided for the RealESRGAN filter");
                return -1;
            }
            processor = std::make_unique<FilterRealesrgan>(
                static_cast<int>(vk_device_index),
                config.tta_mode,
                processor_config->scaling_factor,
                config.model_name
            );
            break;
        }
        case PROCESSOR_RIFE: {
            const auto &config = processor_config->config.rife;
            if (!config.model_name) {
                spdlog::critical("Model name must be provided for the RIFE filter");
                return -1;
            }
            processor = std::make_unique<InterpolatorRIFE>(
                static_cast<int>(vk_device_index),
                config.tta_mode,
                config.tta_temporal_mode,
                config.uhd_mode,
                config.num_threads,
                config.rife_v2,
                config.rife_v4,
                config.model_name
            );
            break;
        }
        default:
            spdlog::critical("Unknown filter type");
            return -1;
    }

    // Check if the filter instance was created successfully
    if (processor == nullptr) {
        spdlog::critical("Failed to create filter instance");
        return -1;
    }

    // Initialize the filter
    ret = processor->init(dec_ctx, encoder.get_encoder_context(), hw_ctx.get());
    if (ret < 0) {
        spdlog::critical("Failed to initialize filter");
        return ret;
    }

    // Process frames using the encoder and decoder
    ret = process_frames(
        encoder_config, processor_config, proc_ctx, decoder, encoder, processor.get(), benchmark
    );
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
