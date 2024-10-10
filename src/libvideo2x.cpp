#include "libvideo2x.h"

#include <libavutil/mathematics.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstdint>
#include <thread>

#include "decoder.h"
#include "encoder.h"
#include "filter.h"
#include "libplacebo_filter.h"
#include "realesrgan_filter.h"

/**
 * @brief Process frames using the selected filter.
 *
 * @param[in,out] proc_ctx Struct containing the processing context
 * @param[in] fmt_ctx Input format context
 * @param[in] ofmt_ctx Output format context
 * @param[in] dec_ctx Decoder context
 * @param[in] enc_ctx Encoder context
 * @param[in] filter Filter instance
 * @param[in] video_stream_index Index of the video stream in the input format context
 * @return int 0 on success, negative value on error
 */
int process_frames(
    EncoderConfig *encoder_config,
    VideoProcessingContext *proc_ctx,
    AVFormatContext *ifmt_ctx,
    AVFormatContext *ofmt_ctx,
    AVCodecContext *dec_ctx,
    AVCodecContext *enc_ctx,
    Filter *filter,
    int video_stream_index,
    int *stream_mapping,
    bool benchmark = false
) {
    int ret;
    AVPacket packet;
    std::vector<AVFrame *> flushed_frames;
    char errbuf[AV_ERROR_MAX_STRING_SIZE];

    // Get the total number of frames in the video
    AVStream *video_stream = ifmt_ctx->streams[video_stream_index];
    proc_ctx->total_frames = video_stream->nb_frames;

    // If nb_frames is not set, calculate total frames using duration and frame rate
    if (proc_ctx->total_frames == 0) {
        int64_t duration = video_stream->duration;
        AVRational frame_rate = video_stream->avg_frame_rate;
        if (duration != AV_NOPTS_VALUE && frame_rate.num != 0 && frame_rate.den != 0) {
            proc_ctx->total_frames = duration * frame_rate.num / frame_rate.den;
        }
    }

    // Get start time
    proc_ctx->start_time = time(NULL);
    if (proc_ctx->start_time == -1) {
        perror("time");
    }

    AVFrame *frame = av_frame_alloc();
    if (frame == nullptr) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    // Read frames from the input file
    while (!proc_ctx->abort) {
        ret = av_read_frame(ifmt_ctx, &packet);
        if (ret < 0) {
            break;  // End of file or error
        }

        if (packet.stream_index == video_stream_index) {
            // Send the packet to the decoder
            ret = avcodec_send_packet(dec_ctx, &packet);
            if (ret < 0) {
                av_strerror(ret, errbuf, sizeof(errbuf));
                fprintf(stderr, "Error sending packet to decoder: %s\n", errbuf);
                av_packet_unref(&packet);
                goto end;
            }

            // Receive and process frames from the decoder
            while (!proc_ctx->abort) {
                // Check if the processing is paused
                if (proc_ctx->pause) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }

                ret = avcodec_receive_frame(dec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    av_strerror(ret, errbuf, sizeof(errbuf));
                    fprintf(stderr, "Error decoding video frame: %s\n", errbuf);
                    goto end;
                }

                // Process the frame using the selected filter
                AVFrame *processed_frame = nullptr;
                ret = filter->process_frame(frame, &processed_frame);
                if (ret == 0 && processed_frame != nullptr) {
                    // Encode and write the processed frame
                    if (!benchmark) {
                        ret = encode_and_write_frame(
                            processed_frame, enc_ctx, ofmt_ctx, video_stream_index
                        );
                        if (ret < 0) {
                            av_strerror(ret, errbuf, sizeof(errbuf));
                            fprintf(stderr, "Error encoding/writing frame: %s\n", errbuf);
                            av_frame_free(&processed_frame);
                            goto end;
                        }
                    }

                    av_frame_free(&processed_frame);
                    proc_ctx->processed_frames++;
                } else if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
                    fprintf(stderr, "Filter returned an error\n");
                    goto end;
                }

                av_frame_unref(frame);
                // TODO: Print the debug processing status
            }
        } else if (encoder_config->copy_streams && stream_mapping[packet.stream_index] >= 0) {
            AVStream *in_stream = ifmt_ctx->streams[packet.stream_index];
            int out_stream_index = stream_mapping[packet.stream_index];
            AVStream *out_stream = ofmt_ctx->streams[out_stream_index];

            // Rescale packet timestamps
            av_packet_rescale_ts(&packet, in_stream->time_base, out_stream->time_base);
            packet.stream_index = out_stream_index;

            // If copy streams is enabled, copy the packet to the output
            ret = av_interleaved_write_frame(ofmt_ctx, &packet);
            if (ret < 0) {
                fprintf(stderr, "Error muxing packet\n");
                av_packet_unref(&packet);
                return ret;
            }
        }
        av_packet_unref(&packet);
    }

    // Flush the filter
    ret = filter->flush(flushed_frames);
    if (ret < 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "Error flushing filter: %s\n", errbuf);
        goto end;
    }

    // Encode and write all flushed frames
    for (AVFrame *&flushed_frame : flushed_frames) {
        ret = encode_and_write_frame(flushed_frame, enc_ctx, ofmt_ctx, video_stream_index);
        if (ret < 0) {
            av_strerror(ret, errbuf, sizeof(errbuf));
            fprintf(stderr, "Error encoding/writing flushed frame: %s\n", errbuf);
            av_frame_free(&flushed_frame);
            flushed_frame = nullptr;
            goto end;
        }
        av_frame_free(&flushed_frame);
        flushed_frame = nullptr;
    }

    // Flush the encoder
    ret = flush_encoder(enc_ctx, ofmt_ctx);
    if (ret < 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "Error flushing encoder: %s\n", errbuf);
        goto end;
    }

end:
    av_frame_free(&frame);
    // Free any flushed frames not yet freed
    for (AVFrame *flushed_frame : flushed_frames) {
        if (flushed_frame) {
            av_frame_free(&flushed_frame);
        }
    }
    return ret;
}

// Cleanup resources after processing the video
void cleanup(
    AVFormatContext *ifmt_ctx,
    AVFormatContext *ofmt_ctx,
    AVCodecContext *dec_ctx,
    AVCodecContext *enc_ctx,
    AVBufferRef *hw_ctx,
    int *stream_mapping,
    Filter *filter
) {
    if (ifmt_ctx) {
        avformat_close_input(&ifmt_ctx);
    }
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&ofmt_ctx->pb);
    }
    if (ofmt_ctx) {
        avformat_free_context(ofmt_ctx);
    }
    if (dec_ctx) {
        avcodec_free_context(&dec_ctx);
    }
    if (enc_ctx) {
        avcodec_free_context(&enc_ctx);
    }
    if (hw_ctx) {
        av_buffer_unref(&hw_ctx);
    }
    if (stream_mapping) {
        av_free(stream_mapping);
    }
    if (filter) {
        delete filter;
    }
}

/**
 * @brief Process a video file using the selected filter and encoder settings.
 *
 * @param[in] input_filename Path to the input video file
 * @param[in] output_filename Path to the output video file
 * @param[in] hw_type Hardware device type
 * @param[in] filter_config Filter configurations
 * @param[in] encoder_config Encoder configurations
 * @param[in,out] proc_ctx Video processing context
 * @return int 0 on success, non-zero value on error
 */
extern "C" int process_video(
    const char *input_filename,
    const char *output_filename,
    bool benchmark,
    AVHWDeviceType hw_type,
    const FilterConfig *filter_config,
    EncoderConfig *encoder_config,
    VideoProcessingContext *proc_ctx
) {
    AVFormatContext *ifmt_ctx = nullptr;
    AVFormatContext *ofmt_ctx = nullptr;
    AVCodecContext *dec_ctx = nullptr;
    AVCodecContext *enc_ctx = nullptr;
    AVBufferRef *hw_ctx = nullptr;
    int *stream_mapping = nullptr;
    Filter *filter = nullptr;
    int video_stream_index = -1;
    int ret = 0;

    // Initialize hardware device context
    if (hw_type != AV_HWDEVICE_TYPE_NONE) {
        ret = av_hwdevice_ctx_create(&hw_ctx, hw_type, NULL, NULL, 0);
        if (ret < 0) {
            fprintf(stderr, "Unable to initialize hardware device context\n");
            return ret;
        }
    }

    // Initialize input
    ret = init_decoder(hw_type, hw_ctx, input_filename, &ifmt_ctx, &dec_ctx, &video_stream_index);
    if (ret < 0) {
        fprintf(stderr, "Failed to initialize decoder\n");
        cleanup(ifmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, hw_ctx, stream_mapping, filter);
        return ret;
    }

    // Initialize output based on Libplacebo or RealESRGAN configuration
    int output_width = 0, output_height = 0;
    switch (filter_config->filter_type) {
        case FILTER_LIBPLACEBO:
            output_width = filter_config->config.libplacebo.output_width;
            output_height = filter_config->config.libplacebo.output_height;
            break;
        case FILTER_REALESRGAN:
            // Calculate the output dimensions based on the scaling factor
            output_width = dec_ctx->width * filter_config->config.realesrgan.scaling_factor;
            output_height = dec_ctx->height * filter_config->config.realesrgan.scaling_factor;
    }

    // Initialize output encoder
    encoder_config->output_width = output_width;
    encoder_config->output_height = output_height;
    ret = init_encoder(
        hw_ctx,
        output_filename,
        ifmt_ctx,
        &ofmt_ctx,
        &enc_ctx,
        dec_ctx,
        encoder_config,
        video_stream_index,
        &stream_mapping
    );
    if (ret < 0) {
        fprintf(stderr, "Failed to initialize encoder\n");
        cleanup(ifmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, hw_ctx, stream_mapping, filter);
        return ret;
    }

    // Write the output file header
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Error occurred when opening output file\n");
        cleanup(ifmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, hw_ctx, stream_mapping, filter);
        return ret;
    }

    // Create and initialize the appropriate filter
    switch (filter_config->filter_type) {
        case FILTER_LIBPLACEBO: {
            const auto &config = filter_config->config.libplacebo;

            // Validate shader path
            if (!config.shader_path) {
                fprintf(stderr, "Shader path must be provided for the libplacebo filter\n");
                cleanup(ifmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, hw_ctx, stream_mapping, filter);
                return -1;
            }

            // Validate output dimensions
            if (config.output_width <= 0 || config.output_height <= 0) {
                fprintf(stderr, "Output dimensions must be provided for the libplacebo filter\n");
                cleanup(ifmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, hw_ctx, stream_mapping, filter);
                return -1;
            }

            filter = new LibplaceboFilter{
                config.output_width, config.output_height, std::filesystem::path(config.shader_path)
            };
            break;
        }
        case FILTER_REALESRGAN: {
            const auto &config = filter_config->config.realesrgan;

            // Validate model name
            if (!config.model) {
                fprintf(stderr, "Model name must be provided for the RealESRGAN filter\n");
                cleanup(ifmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, hw_ctx, stream_mapping, filter);
                return -1;
            }

            // Validate scaling factor
            if (config.scaling_factor <= 0) {
                fprintf(stderr, "Scaling factor must be provided for the RealESRGAN filter\n");
                cleanup(ifmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, hw_ctx, stream_mapping, filter);
                return -1;
            }

            filter = new RealesrganFilter{
                config.gpuid, config.tta_mode, config.scaling_factor, config.model
            };
            break;
        }
        default:
            fprintf(stderr, "Unknown filter type\n");
            cleanup(ifmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, hw_ctx, stream_mapping, filter);
            return -1;
    }

    // Initialize the filter
    ret = filter->init(dec_ctx, enc_ctx, hw_ctx);
    if (ret < 0) {
        fprintf(stderr, "Failed to initialize filter\n");
        cleanup(ifmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, hw_ctx, stream_mapping, filter);
        return ret;
    }

    // Process frames
    ret = process_frames(
        encoder_config,
        proc_ctx,
        ifmt_ctx,
        ofmt_ctx,
        dec_ctx,
        enc_ctx,
        filter,
        video_stream_index,
        stream_mapping,
        benchmark
    );
    if (ret < 0) {
        fprintf(stderr, "Error processing frames\n");
        cleanup(ifmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, hw_ctx, stream_mapping, filter);
        return ret;
    }

    // Write the output file trailer
    av_write_trailer(ofmt_ctx);

    // Cleanup before returning
    cleanup(ifmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, hw_ctx, stream_mapping, filter);

    if (ret < 0 && ret != AVERROR_EOF) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "Error occurred: %s\n", errbuf);
        return ret;
    }
    return 0;
}
