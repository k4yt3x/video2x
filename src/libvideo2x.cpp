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
    process_frame(AVFrame *input_frame, AVFrame *output_frame, AVCodecContext *enc_ctx) = 0;
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

    int process_frame(AVFrame *input_frame, AVFrame *output_frame, AVCodecContext *enc_ctx)
        override {
        int ret;

        // Feed the frame to the filter graph
        ret = av_buffersrc_add_frame(buffersrc_ctx, input_frame);
        if (ret < 0) {
            fprintf(stderr, "Error while feeding the filter graph\n");
            return ret;
        }

        // Get the filtered frame
        while (1) {
            ret = av_buffersink_get_frame(buffersink_ctx, output_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                return 0;  // No frame available right now
            }
            if (ret < 0) {
                return ret;
            }

            output_frame->pict_type = AV_PICTURE_TYPE_NONE;
            // Rescale PTS to encoder's time base
            output_frame->pts = av_rescale_q(
                output_frame->pts, buffersink_ctx->inputs[0]->time_base, enc_ctx->time_base
            );

            // Successfully processed a frame
            return 1;
        }
    }

    int flush(AVCodecContext *enc_ctx, AVFormatContext *ofmt_ctx) override {
        int ret;
        // Signal EOF to the filter graph
        ret = av_buffersrc_add_frame(buffersrc_ctx, nullptr);
        if (ret < 0) {
            fprintf(stderr, "Error while flushing filter graph\n");
            return ret;
        }

        AVFrame *filt_frame = av_frame_alloc();
        if (!filt_frame) {
            return AVERROR(ENOMEM);
        }

        // Get all the remaining frames from the filter graph
        while (1) {
            ret = av_buffersink_get_frame(buffersink_ctx, filt_frame);
            if (ret == AVERROR(EOF) || ret == AVERROR(EAGAIN)) {
                break;
            }
            if (ret < 0) {
                av_frame_free(&filt_frame);
                return ret;
            }

            filt_frame->pict_type = AV_PICTURE_TYPE_NONE;
            filt_frame->pts = av_rescale_q(
                filt_frame->pts, buffersink_ctx->inputs[0]->time_base, enc_ctx->time_base
            );

            // Encode and write the filtered frame
            ret = encode_and_write_frame(filt_frame, enc_ctx, ofmt_ctx);
            if (ret < 0) {
                av_frame_free(&filt_frame);
                return ret;
            }
            av_frame_unref(filt_frame);
        }
        av_frame_free(&filt_frame);

        // Flush the encoder
        ret = flush_encoder(enc_ctx, ofmt_ctx);
        return ret;
    }

    void cleanup() override {
        if (filter_graph) {
            avfilter_graph_free(&filter_graph);
            filter_graph = nullptr;
            buffersrc_ctx = nullptr;
            buffersink_ctx = nullptr;
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

        // Load the model
        if (realesrgan->load(model_param_path, model_bin_path) != 0) {
            fprintf(stderr, "Failed to load RealESRGAN model\n");
            return -1;
        }

        // Set RealESRGAN parameters
        realesrgan->scale = 4;        // Assuming scale factor is 4
        realesrgan->tilesize = 0;     // Auto tilesize
        realesrgan->prepadding = 10;  // Prepadding as per the model

        return 0;
    }

    int process_frame(AVFrame *input_frame, AVFrame *output_frame, AVCodecContext *enc_ctx)
        override {
        // Convert AVFrame to ncnn::Mat
        ncnn::Mat inimage = avframe_to_ncnn_mat(input_frame);
        if (inimage.empty()) {
            fprintf(stderr, "Failed to convert AVFrame to ncnn::Mat\n");
            return -1;
        }

        // Process with RealESRGAN
        ncnn::Mat outimage;
        if (realesrgan->process(inimage, outimage) != 0) {
            fprintf(stderr, "RealESRGAN processing failed\n");
            return -1;
        }

        // Convert ncnn::Mat back to AVFrame
        if (ncnn_mat_to_avframe(outimage, output_frame, enc_ctx->pix_fmt) != 0) {
            fprintf(stderr, "Failed to convert ncnn::Mat to AVFrame\n");
            return -1;
        }

        // Rescale PTS to encoder's time base
        output_frame->pts =
            av_rescale_q(input_frame->pts, enc_ctx->pkt_timebase, enc_ctx->time_base);

        // Indicate that a frame is ready
        return 1;
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
        // Assuming input is in AV_PIX_FMT_RGB24 or AV_PIX_FMT_BGR24

        int width = frame->width;
        int height = frame->height;
        int pix_fmt = frame->format;

        ncnn::Mat ncnn_image;

        if (pix_fmt == AV_PIX_FMT_RGB24) {
            // Create ncnn::Mat from AVFrame data (RGB)
            ncnn_image =
                ncnn::Mat::from_pixels(frame->data[0], ncnn::Mat::PIXEL_RGB, width, height);
        } else if (pix_fmt == AV_PIX_FMT_BGR24) {
            // Create ncnn::Mat from AVFrame data (BGR)
            ncnn_image =
                ncnn::Mat::from_pixels(frame->data[0], ncnn::Mat::PIXEL_BGR, width, height);
        } else if (pix_fmt == AV_PIX_FMT_RGBA) {
            // Create ncnn::Mat from AVFrame data (RGBA)
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

        return ncnn_image;
    }

    int ncnn_mat_to_avframe(const ncnn::Mat &ncnn_image, AVFrame *frame, AVPixelFormat pix_fmt) {
        // Convert ncnn::Mat back to AVFrame
        int width = ncnn_image.w;
        int height = ncnn_image.h;
        int channels = ncnn_image.c;

        // Allocate frame buffer
        frame->format = pix_fmt;
        frame->width = width;
        frame->height = height;

        int ret = av_frame_get_buffer(frame, 32);  // Align to 32 bytes
        if (ret < 0) {
            fprintf(stderr, "Could not allocate frame data.\n");
            return ret;
        }

        // Copy data from ncnn::Mat to AVFrame
        if (pix_fmt == AV_PIX_FMT_RGB24) {
            ncnn_image.to_pixels(frame->data[0], ncnn::Mat::PIXEL_RGB);
        } else if (pix_fmt == AV_PIX_FMT_BGR24) {
            ncnn_image.to_pixels(frame->data[0], ncnn::Mat::PIXEL_BGR);
        } else if (pix_fmt == AV_PIX_FMT_RGBA) {
            ncnn_image.to_pixels(frame->data[0], ncnn::Mat::PIXEL_RGBA);
        } else {
            fprintf(
                stderr, "Unsupported pixel format for output: %s\n", av_get_pix_fmt_name(pix_fmt)
            );
            return -1;
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
    AVFrame *filtered_frame = av_frame_alloc();

    if (!frame || !filtered_frame) {
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

            while (ret >= 0) {
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
                ret = filter->process_frame(frame, filtered_frame, enc_ctx);
                if (ret < 0) {
                    fprintf(stderr, "Error processing frame\n");
                    goto end;
                }
                if (ret == 1) {
                    // Encode and write the filtered frame
                    ret = encode_and_write_frame(filtered_frame, enc_ctx, ofmt_ctx);
                    if (ret < 0) {
                        goto end;
                    }
                    av_frame_unref(filtered_frame);
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
    av_frame_free(&filtered_frame);
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
        filter = new RealesrganFilter();  // You can pass gpuid and tta_mode if needed
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
