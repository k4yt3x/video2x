#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstdint>

// FFmpeg headers
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "decoder.h"
#include "encoder.h"
#include "filter.h"
#include "libplacebo_filter.h"
#include "libvideo2x.h"
#include "realesrgan_filter.h"

// Function to process frames using the selected filter (same as before)
int process_frames(
    ProcessingStatus *status,
    AVFormatContext *fmt_ctx,
    AVFormatContext *ofmt_ctx,
    AVCodecContext *dec_ctx,
    AVCodecContext *enc_ctx,
    Filter *filter,
    int video_stream_index
) {
    int ret;
    AVPacket packet;
    std::vector<AVFrame *> flushed_frames;

    // Get the total number of frames in the video
    AVStream *video_stream = fmt_ctx->streams[video_stream_index];
    status->total_frames = video_stream->nb_frames;

    // If nb_frames is not set, calculate total frames using duration and frame rate
    if (status->total_frames == 0) {
        int64_t duration = video_stream->duration;
        AVRational frame_rate = video_stream->avg_frame_rate;
        if (duration != AV_NOPTS_VALUE && frame_rate.num != 0 && frame_rate.den != 0) {
            status->total_frames = duration * frame_rate.num / frame_rate.den;
        }
    }

    // Get start time
    status->start_time = time(NULL);
    if (status->start_time == -1) {
        perror("time");
    }

    AVFrame *frame = av_frame_alloc();
    if (frame == nullptr) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    // Read frames from the input file
    while (1) {
        ret = av_read_frame(fmt_ctx, &packet);
        if (ret < 0) {
            break;  // End of file or error
        }

        if (packet.stream_index == video_stream_index) {
            // Send the packet to the decoder
            ret = avcodec_send_packet(dec_ctx, &packet);
            if (ret < 0) {
                fprintf(stderr, "Error sending packet to decoder: %s\n", av_err2str(ret));
                av_packet_unref(&packet);
                goto end;
            }

            // Receive and process frames from the decoder
            while (1) {
                ret = avcodec_receive_frame(dec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    fprintf(stderr, "Error decoding video frame: %s\n", av_err2str(ret));
                    goto end;
                }

                // Process the frame using the selected filter
                AVFrame *processed_frame = filter->process_frame(frame);
                if (processed_frame != nullptr && processed_frame != (AVFrame *)-1) {
                    // Encode and write the processed frame
                    ret = encode_and_write_frame(processed_frame, enc_ctx, ofmt_ctx);
                    if (ret < 0) {
                        fprintf(stderr, "Error encoding/writing frame: %s\n", av_err2str(ret));
                        av_frame_free(&processed_frame);
                        goto end;
                    }

                    av_frame_free(&processed_frame);
                    status->processed_frames++;
                } else if (processed_frame != (AVFrame *)-1) {
                    fprintf(stderr, "Error processing frame\n");
                    goto end;
                }

                av_frame_unref(frame);

                // Print the processing status
                printf(
                    "\r[Video2X] Processing frame %ld/%ld (%.2f%%); time elapsed: %lds",
                    status->processed_frames,
                    status->total_frames,
                    status->processed_frames * 100.0 / status->total_frames,
                    time(NULL) - status->start_time
                );
                fflush(stdout);
            }
        }
        av_packet_unref(&packet);
    }

    // Print a newline after processing all frames
    printf("\n");

    // Flush the filter
    ret = filter->flush(flushed_frames);
    if (ret < 0) {
        fprintf(stderr, "Error flushing filter: %s\n", av_err2str(ret));
        goto end;
    }

    // Encode and write all flushed frames
    for (AVFrame *&flushed_frame : flushed_frames) {
        ret = encode_and_write_frame(flushed_frame, enc_ctx, ofmt_ctx);
        if (ret < 0) {
            fprintf(stderr, "Error encoding/writing flushed frame: %s\n", av_err2str(ret));
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
        fprintf(stderr, "Error flushing encoder: %s\n", av_err2str(ret));
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

// Cleanup helper function
void cleanup(
    AVFormatContext *fmt_ctx,
    AVFormatContext *ofmt_ctx,
    AVCodecContext *dec_ctx,
    AVCodecContext *enc_ctx,
    Filter *filter
) {
    if (filter) {
        delete filter;
    }
    if (dec_ctx) {
        avcodec_free_context(&dec_ctx);
    }
    if (enc_ctx) {
        avcodec_free_context(&enc_ctx);
    }
    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
    }
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&ofmt_ctx->pb);
    }
    if (ofmt_ctx) {
        avformat_free_context(ofmt_ctx);
    }
}

// Main function to process the video
extern "C" int process_video(
    const char *input_filename,
    const char *output_filename,
    const FilterConfig *filter_config,
    EncoderConfig *encoder_config,
    ProcessingStatus *status
) {
    AVFormatContext *fmt_ctx = nullptr;
    AVFormatContext *ofmt_ctx = nullptr;
    AVCodecContext *dec_ctx = nullptr;
    AVCodecContext *enc_ctx = nullptr;
    Filter *filter = nullptr;
    int video_stream_index = -1;
    int ret = 0;  // Initialize ret with 0 to assume success

    // Initialize input
    if (init_decoder(input_filename, &fmt_ctx, &dec_ctx, &video_stream_index) < 0) {
        fprintf(stderr, "Failed to initialize decoder\n");
        cleanup(fmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, filter);
        return 1;
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
    if (init_encoder(output_filename, &ofmt_ctx, &enc_ctx, dec_ctx, encoder_config) < 0) {
        fprintf(stderr, "Failed to initialize encoder\n");
        cleanup(fmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, filter);
        return 1;
    }

    // Write the output file header
    if (avformat_write_header(ofmt_ctx, NULL) < 0) {
        fprintf(stderr, "Error occurred when opening output file\n");
        cleanup(fmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, filter);
        return 1;
    }

    // Create and initialize the appropriate filter
    switch (filter_config->filter_type) {
        case FILTER_LIBPLACEBO: {
            const auto &config = filter_config->config.libplacebo;

            // Validate shader path
            if (!config.shader_path) {
                fprintf(stderr, "Shader path must be provided for the libplacebo filter\n");
                cleanup(fmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, filter);
                return 1;
            }

            // Validate output dimensions
            if (config.output_width <= 0 || config.output_height <= 0) {
                fprintf(stderr, "Output dimensions must be provided for the libplacebo filter\n");
                cleanup(fmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, filter);
                return 1;
            }

            filter =
                new LibplaceboFilter(config.output_width, config.output_height, config.shader_path);
            break;
        }
        case FILTER_REALESRGAN: {
            const auto &config = filter_config->config.realesrgan;

            // Validate model name
            if (!config.model) {
                fprintf(stderr, "Model name must be provided for the RealESRGAN filter\n");
                cleanup(fmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, filter);
                return 1;
            }

            // Validate scaling factor
            if (config.scaling_factor <= 0) {
                fprintf(stderr, "Scaling factor must be provided for the RealESRGAN filter\n");
                cleanup(fmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, filter);
                return 1;
            }

            filter = new RealesrganFilter(config.gpuid, config.tta_mode);
            break;
        }
        default:
            fprintf(stderr, "Unknown filter type\n");
            cleanup(fmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, filter);
            return 1;
    }

    // Initialize the filter
    if (filter->init(dec_ctx, enc_ctx) < 0) {
        fprintf(stderr, "Failed to initialize filter\n");
        cleanup(fmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, filter);
        return 1;
    }

    // Process frames
    if ((ret =
             process_frames(status, fmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, filter, video_stream_index)
        ) < 0) {
        fprintf(stderr, "Error processing frames\n");
        cleanup(fmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, filter);
        return 1;
    }

    // Write the output file trailer
    av_write_trailer(ofmt_ctx);

    // Cleanup before returning
    cleanup(fmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, filter);

    if (ret < 0 && ret != AVERROR_EOF) {
        fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));
        return 1;
    }
    return 0;
}
