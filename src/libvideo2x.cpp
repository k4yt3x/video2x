#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// FFmpeg includes
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/rational.h>
#include <libswscale/swscale.h>
}

#include "decoder.h"
#include "encoder.h"
#include "libvideo2x.h"
#include "placebo.h"
#include "realesrgan.h"

// ncnn includes
#include <gpu.h>
#include <mat.h>

// Abstract base class for filters
class Filter {
   public:
    virtual int init(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx) = 0;
    virtual int
    process_frame(AVFrame *input_frame, AVCodecContext *enc_ctx, AVFormatContext *ofmt_ctx) = 0;
    virtual int flush(AVCodecContext *enc_ctx, AVFormatContext *ofmt_ctx) = 0;
    virtual void cleanup() = 0;
    virtual ~Filter() {}
};

// Libplacebo filter implementation
class LibplaceboFilter : public Filter {
   private:
    AVFilterGraph *filter_graph;
    AVFilterContext *buffersrc_ctx;
    AVFilterContext *buffersink_ctx;
    int output_width;
    int output_height;
    const char *shader_path;

   public:
    LibplaceboFilter(int width, int height, const char *shader)
        : filter_graph(nullptr),
          buffersrc_ctx(nullptr),
          buffersink_ctx(nullptr),
          output_width(width),
          output_height(height),
          shader_path(shader) {}

    virtual ~LibplaceboFilter() { cleanup(); }

    int init(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx) override {
        // Initialize the filter graph using libplacebo
        int ret = init_libplacebo(
            &filter_graph,
            &buffersrc_ctx,
            &buffersink_ctx,
            dec_ctx,
            output_width,
            output_height,
            shader_path
        );
        return ret;
    }

    int process_frame(AVFrame *input_frame, AVCodecContext *enc_ctx, AVFormatContext *ofmt_ctx)
        override {
        int ret;

        // Feed the frame to the filter graph
        ret = av_buffersrc_add_frame(buffersrc_ctx, input_frame);
        if (ret < 0) {
            fprintf(stderr, "Error while feeding the filter graph\n");
            return ret;
        }

        // Get the filtered frames
        while (1) {
            AVFrame *output_frame = av_frame_alloc();
            if (!output_frame) {
                return AVERROR(ENOMEM);
            }

            ret = av_buffersink_get_frame(buffersink_ctx, output_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                av_frame_free(&output_frame);
                break;
            }
            if (ret < 0) {
                av_frame_free(&output_frame);
                return ret;
            }

            output_frame->pict_type = AV_PICTURE_TYPE_NONE;

            // Rescale PTS to encoder's time base
            output_frame->pts = av_rescale_q(
                output_frame->pts, buffersink_ctx->inputs[0]->time_base, enc_ctx->time_base
            );

            // Encode and write the filtered frame
            ret = encode_and_write_frame(output_frame, enc_ctx, ofmt_ctx);
            if (ret < 0) {
                av_frame_free(&output_frame);
                return ret;
            }

            av_frame_free(&output_frame);
        }

        return 0;
    }

    int flush(AVCodecContext *enc_ctx, AVFormatContext *ofmt_ctx) override {
        int ret;
        // Signal EOF to the filter graph
        ret = av_buffersrc_add_frame(buffersrc_ctx, nullptr);
        if (ret < 0) {
            fprintf(stderr, "Error while flushing filter graph\n");
            return ret;
        }

        // Get all remaining frames from the filter graph
        while (1) {
            AVFrame *filt_frame = av_frame_alloc();
            if (!filt_frame) {
                return AVERROR(ENOMEM);
            }

            ret = av_buffersink_get_frame(buffersink_ctx, filt_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                av_frame_free(&filt_frame);
                break;
            }
            if (ret < 0) {
                av_frame_free(&filt_frame);
                return ret;
            }

            filt_frame->pict_type = AV_PICTURE_TYPE_NONE;

            // Rescale PTS to encoder's time base
            filt_frame->pts = av_rescale_q(
                filt_frame->pts, buffersink_ctx->inputs[0]->time_base, enc_ctx->time_base
            );

            // Encode and write the filtered frame
            ret = encode_and_write_frame(filt_frame, enc_ctx, ofmt_ctx);
            if (ret < 0) {
                av_frame_free(&filt_frame);
                return ret;
            }

            av_frame_free(&filt_frame);
        }

        // Flush the encoder
        ret = flush_encoder(enc_ctx, ofmt_ctx);
        return ret;
    }

    void cleanup() override {
        if (buffersrc_ctx) {
            avfilter_free(buffersrc_ctx);
            buffersrc_ctx = nullptr;
        }
        if (buffersink_ctx) {
            avfilter_free(buffersink_ctx);
            buffersink_ctx = nullptr;
        }
        if (filter_graph) {
            avfilter_graph_free(&filter_graph);
            filter_graph = nullptr;
        }
    }
};

// Realesrgan filter implementation
class RealesrganFilter : public Filter {
   private:
    RealESRGAN *realesrgan;
    int gpuid;
    bool tta_mode;
    std::string model_param_path;
    std::string model_bin_path;
    AVRational input_time_base;
    AVRational output_time_base;

   public:
    RealesrganFilter(int gpuid = 0, bool tta_mode = false)
        : realesrgan(nullptr), gpuid(gpuid), tta_mode(tta_mode) {}

    virtual ~RealesrganFilter() { cleanup(); }

    int init(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx) override {
        // Initialize ncnn GPU instance
        ncnn::create_gpu_instance();

        // Initialize RealESRGAN
        realesrgan = new RealESRGAN(gpuid, tta_mode);

        // Set default model paths or accept them as parameters
        model_param_path = "models/realesr-animevideov3-x4.param";
        model_bin_path = "models/realesr-animevideov3-x4.bin";

        // Store the time bases
        input_time_base = dec_ctx->time_base;
        output_time_base = enc_ctx->time_base;

        // Load the model
        if (realesrgan->load(model_param_path, model_bin_path) != 0) {
            fprintf(stderr, "Failed to load RealESRGAN model\n");
            return -1;
        }

        // TODO: Set RealESRGAN parameters programmatically
        // Set RealESRGAN parameters
        realesrgan->scale = 4;
        realesrgan->tilesize = 200;
        realesrgan->prepadding = 10;

        return 0;
    }

    int process_frame(AVFrame *input_frame, AVCodecContext *enc_ctx, AVFormatContext *ofmt_ctx)
        override {
        // Convert AVFrame to ncnn::Mat
        ncnn::Mat inimage = avframe_to_ncnn_mat(input_frame);
        if (inimage.empty()) {
            fprintf(stderr, "Failed to convert AVFrame to ncnn::Mat\n");
            return -1;
        }

        // Process with RealESRGAN
        ncnn::Mat outimage = ncnn::Mat(
            input_frame->width * realesrgan->scale, input_frame->height * realesrgan->scale, 3
        );
        if (realesrgan->process(inimage, outimage) != 0) {
            fprintf(stderr, "RealESRGAN processing failed\n");
            return -1;
        }

        // Convert ncnn::Mat back to AVFrame
        AVFrame *output_frame = av_frame_alloc();
        if (!output_frame) {
            fprintf(stderr, "Could not allocate output frame\n");
            return AVERROR(ENOMEM);
        }

        if (ncnn_mat_to_avframe(outimage, output_frame, enc_ctx->pix_fmt) != 0) {
            fprintf(stderr, "Failed to convert ncnn::Mat to AVFrame\n");
            av_frame_free(&output_frame);
            return -1;
        }

        // Rescale PTS to encoder's time base
        output_frame->pts = av_rescale_q(input_frame->pts, input_time_base, enc_ctx->time_base);

        // Encode and write the output frame
        int ret = encode_and_write_frame(output_frame, enc_ctx, ofmt_ctx);
        if (ret < 0) {
            av_frame_free(&output_frame);
            return ret;
        }

        av_frame_free(&output_frame);

        return 0;
    }

    int flush(AVCodecContext *enc_ctx, AVFormatContext *ofmt_ctx) override {
        // Flush the encoder
        int ret = flush_encoder(enc_ctx, ofmt_ctx);
        return ret;
    }

    void cleanup() override {
        if (realesrgan) {
            delete realesrgan;
            realesrgan = nullptr;
            ncnn::destroy_gpu_instance();
        }
    }

   private:
    ncnn::Mat avframe_to_ncnn_mat(AVFrame *frame) {
        // Convert AVFrame to ncnn::Mat

        int width = frame->width;
        int height = frame->height;
        int pix_fmt = frame->format;

        ncnn::Mat ncnn_image;

        // Choose the target pixel format (AV_PIX_FMT_RGB24 or AV_PIX_FMT_BGR24)
        AVPixelFormat target_pix_fmt = AV_PIX_FMT_BGR24;  // or AV_PIX_FMT_RGB24

        // Check if conversion is needed
        if (pix_fmt != target_pix_fmt) {
            // Set up sws context for conversion
            struct SwsContext *sws_ctx = sws_getContext(
                width,
                height,
                (AVPixelFormat)pix_fmt,
                width,
                height,
                target_pix_fmt,
                SWS_BILINEAR,
                NULL,
                NULL,
                NULL
            );

            if (!sws_ctx) {
                fprintf(stderr, "Could not initialize sws context\n");
                return ncnn::Mat();
            }

            // Allocate a frame to hold the converted image
            AVFrame *converted_frame = av_frame_alloc();
            if (!converted_frame) {
                fprintf(stderr, "Could not allocate converted frame\n");
                sws_freeContext(sws_ctx);
                return ncnn::Mat();
            }

            converted_frame->format = target_pix_fmt;
            converted_frame->width = width;
            converted_frame->height = height;

            // Allocate buffer for the converted frame
            int ret = av_frame_get_buffer(converted_frame, 32);  // 32-byte alignment
            if (ret < 0) {
                fprintf(stderr, "Could not allocate frame data\n");
                av_frame_free(&converted_frame);
                sws_freeContext(sws_ctx);
                return ncnn::Mat();
            }

            // Perform the conversion
            ret = sws_scale(
                sws_ctx,
                frame->data,
                frame->linesize,
                0,
                height,
                converted_frame->data,
                converted_frame->linesize
            );

            if (ret < 0) {
                fprintf(stderr, "Error converting pixel format\n");
                av_frame_free(&converted_frame);
                sws_freeContext(sws_ctx);
                return ncnn::Mat();
            }

            // Create ncnn::Mat from the converted frame
            if (target_pix_fmt == AV_PIX_FMT_RGB24) {
                ncnn_image = ncnn::Mat::from_pixels(
                    converted_frame->data[0], ncnn::Mat::PIXEL_RGB, width, height
                );
            } else if (target_pix_fmt == AV_PIX_FMT_BGR24) {
                ncnn_image = ncnn::Mat::from_pixels(
                    converted_frame->data[0], ncnn::Mat::PIXEL_BGR, width, height
                );
            } else if (target_pix_fmt == AV_PIX_FMT_RGBA) {
                ncnn_image = ncnn::Mat::from_pixels(
                    converted_frame->data[0], ncnn::Mat::PIXEL_RGBA, width, height
                );
            } else {
                fprintf(stderr, "Unsupported target pixel format for ncnn::Mat\n");
                av_frame_free(&converted_frame);
                sws_freeContext(sws_ctx);
                return ncnn::Mat();
            }

            // Clean up
            av_frame_free(&converted_frame);
            sws_freeContext(sws_ctx);
        } else {
            // If the pixel format is already supported, create ncnn::Mat directly
            if (pix_fmt == AV_PIX_FMT_RGB24) {
                ncnn_image =
                    ncnn::Mat::from_pixels(frame->data[0], ncnn::Mat::PIXEL_RGB, width, height);
            } else if (pix_fmt == AV_PIX_FMT_BGR24) {
                ncnn_image =
                    ncnn::Mat::from_pixels(frame->data[0], ncnn::Mat::PIXEL_BGR, width, height);
            } else if (pix_fmt == AV_PIX_FMT_RGBA) {
                ncnn_image =
                    ncnn::Mat::from_pixels(frame->data[0], ncnn::Mat::PIXEL_RGBA, width, height);
            } else {
                fprintf(
                    stderr,
                    "Unsupported pixel format for RealESRGAN: %s\n",
                    av_get_pix_fmt_name((AVPixelFormat)pix_fmt)
                );
                return ncnn::Mat();
            }
        }

        return ncnn_image;
    }

    int ncnn_mat_to_avframe(const ncnn::Mat &ncnn_image, AVFrame *frame, AVPixelFormat pix_fmt) {
        int width = ncnn_image.w;
        int height = ncnn_image.h;

        // Allocate frame buffer for the RGB frame
        AVFrame *rgb_frame = av_frame_alloc();
        if (!rgb_frame) {
            fprintf(stderr, "Could not allocate RGB frame\n");
            return AVERROR(ENOMEM);
        }
        rgb_frame->format = AV_PIX_FMT_RGB24;
        rgb_frame->width = width;
        rgb_frame->height = height;

        int ret = av_frame_get_buffer(rgb_frame, 32);  // Align to 32 bytes
        if (ret < 0) {
            fprintf(stderr, "Could not allocate RGB frame data.\n");
            av_frame_free(&rgb_frame);
            return ret;
        }

        // Copy data from ncnn::Mat to RGB AVFrame
        // Assuming ncnn_image is in PIXEL_RGB format
        ncnn_image.to_pixels(rgb_frame->data[0], ncnn::Mat::PIXEL_RGB);

        if (pix_fmt != AV_PIX_FMT_RGB24 && pix_fmt != AV_PIX_FMT_BGR24 &&
            pix_fmt != AV_PIX_FMT_RGBA) {
            // Convert RGB frame to desired pixel format (e.g., YUV420P) using sws_scale
            // Allocate frame buffer for the output frame
            frame->format = pix_fmt;
            frame->width = width;
            frame->height = height;

            ret = av_frame_get_buffer(frame, 32);  // Align to 32 bytes
            if (ret < 0) {
                fprintf(stderr, "Could not allocate output frame data.\n");
                av_frame_free(&rgb_frame);
                return ret;
            }

            // Set up sws context for conversion
            struct SwsContext *sws_ctx = sws_getContext(
                width,
                height,
                AV_PIX_FMT_RGB24,  // Source format
                width,
                height,
                pix_fmt,  // Destination format
                SWS_BILINEAR,
                NULL,
                NULL,
                NULL
            );

            if (!sws_ctx) {
                fprintf(
                    stderr, "Could not initialize sws context for RGB to output format conversion\n"
                );
                av_frame_free(&rgb_frame);
                return AVERROR(EINVAL);
            }

            // Perform the conversion
            ret = sws_scale(
                sws_ctx,
                rgb_frame->data,
                rgb_frame->linesize,
                0,
                height,
                frame->data,
                frame->linesize
            );

            if (ret < 0) {
                fprintf(stderr, "Error converting RGB to output pixel format\n");
                av_frame_free(&rgb_frame);
                sws_freeContext(sws_ctx);
                return ret;
            }

            // Clean up
            av_frame_free(&rgb_frame);
            sws_freeContext(sws_ctx);
        } else {
            // If the desired output format is RGB24, we can directly use rgb_frame
            // Copy the data from rgb_frame to frame
            ret = av_frame_copy(frame, rgb_frame);
            if (ret < 0) {
                fprintf(stderr, "Could not copy RGB frame data to output frame.\n");
                av_frame_free(&rgb_frame);
                return ret;
            }
            av_frame_free(&rgb_frame);
        }

        return 0;
    }
};

// Function to process frames using the selected filter (same as before)
int process_frames(
    AVFormatContext *fmt_ctx,
    AVFormatContext *ofmt_ctx,
    AVCodecContext *dec_ctx,
    AVCodecContext *enc_ctx,
    Filter *filter,
    int video_stream_index
) {
    int ret;
    AVPacket packet;
    AVFrame *frame = av_frame_alloc();

    if (!frame) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    while (1) {
        ret = av_read_frame(fmt_ctx, &packet);
        if (ret < 0) {
            break;  // End of file or error
        }

        if (packet.stream_index == video_stream_index) {
            ret = avcodec_send_packet(dec_ctx, &packet);
            if (ret < 0) {
                fprintf(stderr, "Error sending packet to decoder\n");
                av_packet_unref(&packet);
                break;
            }

            while (1) {
                ret = avcodec_receive_frame(dec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    fprintf(stderr, "Error decoding video frame\n");
                    goto end;
                }

                // Set the frame PTS if necessary
                if (frame->pts == AV_NOPTS_VALUE) {
                    frame->pts = frame->best_effort_timestamp;
                }

                // Process the frame using the selected filter
                ret = filter->process_frame(frame, enc_ctx, ofmt_ctx);
                if (ret < 0) {
                    fprintf(stderr, "Error processing frame\n");
                    goto end;
                }

                av_frame_unref(frame);
            }
        }
        av_packet_unref(&packet);
    }

    // Flush the filter and the encoder
    ret = filter->flush(enc_ctx, ofmt_ctx);
    if (ret < 0) {
        goto end;
    }

end:
    av_frame_free(&frame);
    return ret;
}

// Main function to process the video
int process_video(
    const char *input_filename,
    const char *output_filename,
    int output_width,
    int output_height,
    FilterType filter_type,
    const char *shader_path = nullptr
) {
    AVFormatContext *fmt_ctx = nullptr;
    AVFormatContext *ofmt_ctx = nullptr;
    AVCodecContext *dec_ctx = nullptr;
    AVCodecContext *enc_ctx = nullptr;
    Filter *filter = nullptr;
    int video_stream_index = -1;
    int ret;

#ifdef DEBUG
    av_log_set_level(AV_LOG_DEBUG);
#endif

    // Initialize input
    if ((ret = init_decoder(input_filename, &fmt_ctx, &dec_ctx, &video_stream_index)) < 0) {
        goto end;
    }

    // Initialize output
    if ((ret = init_encoder(
             output_filename, &ofmt_ctx, &enc_ctx, dec_ctx, output_width, output_height
         )) < 0) {
        goto end;
    }

    // Write the output file header
    if ((ret = avformat_write_header(ofmt_ctx, NULL)) < 0) {
        fprintf(stderr, "Error occurred when opening output file\n");
        goto end;
    }

    // Create and initialize the filter
    if (filter_type == FILTER_LIBPLACEBO) {
        if (!shader_path) {
            fprintf(stderr, "Shader path must be provided for Libplacebo filter\n");
            ret = -1;
            goto end;
        }
        filter = new LibplaceboFilter(output_width, output_height, shader_path);
    } else if (filter_type == FILTER_REALESRGAN) {
        filter = new RealesrganFilter();
    } else {
        fprintf(stderr, "Unknown filter type\n");
        ret = -1;
        goto end;
    }

    if ((ret = filter->init(dec_ctx, enc_ctx)) < 0) {
        goto end;
    }

    // Process frames
    if ((ret = process_frames(fmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, filter, video_stream_index)) <
        0) {
        goto end;
    }

    // Write the output file trailer
    av_write_trailer(ofmt_ctx);

end:
    // Cleanup
    if (filter) {
        filter->cleanup();
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

    if (ret < 0 && ret != AVERROR_EOF) {
        fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));
        return 1;
    }

    return 0;
}
