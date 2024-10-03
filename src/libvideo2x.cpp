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
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
#include <libswscale/swscale.h>
}

// ncnn includes
#include <ncnn/mat.h>

#include "conversions.h"
#include "decoder.h"
#include "encoder.h"
#include "libvideo2x.h"
#include "placebo.h"
#include "realesrgan.h"

// Abstract base class for filters
class Filter {
   public:
    virtual ~Filter() {}
    virtual int init(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx) = 0;
    virtual AVFrame *process_frame(AVFrame *input_frame) = 0;
    virtual int flush(std::vector<AVFrame *> &processed_frames) = 0;
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
    AVRational output_time_base;

   public:
    LibplaceboFilter(int width, int height, const char *shader)
        : filter_graph(nullptr),
          buffersrc_ctx(nullptr),
          buffersink_ctx(nullptr),
          output_width(width),
          output_height(height),
          shader_path(shader) {}

    virtual ~LibplaceboFilter() {
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

    int init(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx) override {
        // Save the output time base
        output_time_base = enc_ctx->time_base;

        return init_libplacebo(
            &filter_graph,
            &buffersrc_ctx,
            &buffersink_ctx,
            dec_ctx,
            output_width,
            output_height,
            shader_path
        );
    }

    AVFrame *process_frame(AVFrame *input_frame) override {
        int ret;

        // Get the filtered frame
        AVFrame *output_frame = av_frame_alloc();
        if (output_frame == nullptr) {
            fprintf(stderr, "Failed to allocate output frame\n");
            return nullptr;
        }

        // Feed the frame to the filter graph
        ret = av_buffersrc_add_frame(buffersrc_ctx, input_frame);
        if (ret < 0) {
            fprintf(stderr, "Error while feeding the filter graph\n");
            return nullptr;
        }

        ret = av_buffersink_get_frame(buffersink_ctx, output_frame);
        if (ret < 0) {
            av_frame_free(&output_frame);
            if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
                fprintf(stderr, "Error getting frame from filter graph: %s\n", av_err2str(ret));
                return nullptr;
            }
            return (AVFrame *)-1;
        }

        // Rescale PTS to encoder's time base
        output_frame->pts =
            av_rescale_q(output_frame->pts, buffersink_ctx->inputs[0]->time_base, output_time_base);

        // Return the processed frame to the caller
        return output_frame;
    }

    int flush(std::vector<AVFrame *> &processed_frames) override {
        int ret = av_buffersrc_add_frame(buffersrc_ctx, nullptr);  // Signal EOF to the filter graph
        if (ret < 0) {
            fprintf(stderr, "Error while flushing filter graph\n");
            return ret;
        }

        // Retrieve all remaining frames from the filter graph
        while (1) {
            AVFrame *filt_frame = av_frame_alloc();
            if (filt_frame == nullptr) {
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

            // Rescale PTS to encoder's time base
            filt_frame->pts = av_rescale_q(
                filt_frame->pts, buffersink_ctx->inputs[0]->time_base, output_time_base
            );

            // Add to processed frames
            processed_frames.push_back(filt_frame);
        }

        return 0;
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
    AVPixelFormat output_pix_fmt;

   public:
    RealesrganFilter(int gpuid = 0, bool tta_mode = false)
        : realesrgan(nullptr), gpuid(gpuid), tta_mode(tta_mode) {}

    virtual ~RealesrganFilter() {
        if (realesrgan) {
            delete realesrgan;
            realesrgan = nullptr;
        }
    }

    int init(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx) override {
        realesrgan = new RealESRGAN(gpuid, tta_mode);
        model_param_path = "models/realesr-animevideov3-x4.param";
        model_bin_path = "models/realesr-animevideov3-x4.bin";

        // Store the time bases
        input_time_base = dec_ctx->time_base;
        output_time_base = enc_ctx->time_base;
        output_pix_fmt = enc_ctx->pix_fmt;

        // Load the model
        if (realesrgan->load(model_param_path, model_bin_path) != 0) {
            fprintf(stderr, "Failed to load RealESRGAN model\n");
            return -1;
        }

        // TODO: Set these values programmatically
        // Set RealESRGAN parameters
        realesrgan->scale = 4;
        realesrgan->tilesize = 200;
        realesrgan->prepadding = 10;

        return 0;
    }

    AVFrame *process_frame(AVFrame *input_frame) override {
        // Convert the input frame to RGB24
        ncnn::Mat input_mat = avframe_to_ncnn_mat(input_frame);
        if (input_mat.empty()) {
            fprintf(stderr, "Failed to convert AVFrame to ncnn::Mat\n");
            return nullptr;
        }

        // Allocate space for ouptut ncnn::Mat
        ncnn::Mat output_mat =
            ncnn::Mat(input_mat.w * realesrgan->scale, input_mat.h * realesrgan->scale, 3);

        if (realesrgan->process(input_mat, output_mat) != 0) {
            fprintf(stderr, "RealESRGAN processing failed\n");
            return nullptr;
        }

        // Convert ncnn::Mat to AVFrame
        AVFrame *output_frame = ncnn_mat_to_avframe(output_mat, output_pix_fmt);

        // Rescale PTS to encoder's time base
        output_frame->pts = av_rescale_q(input_frame->pts, input_time_base, output_time_base);

        // Return the processed frame to the caller
        return output_frame;
    }

    int flush(std::vector<AVFrame *> &processed_frames) override {
        // No special flushing needed for RealESRGAN
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
    std::vector<AVFrame *> flushed_frames;

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
                } else if (processed_frame != (AVFrame *)-1) {
                    fprintf(stderr, "Error processing frame\n");
                    goto end;
                }

                av_frame_unref(frame);

                // TODO: remove this
                printf(".");
                fflush(stdout);
            }
        }
        av_packet_unref(&packet);
    }
    // TODO: remove this
    printf("\n");
    fflush(stdout);

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

    // Enable FFmpeg debug logging
    // av_log_set_level(AV_LOG_DEBUG);

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
    switch (filter_type) {
        case FILTER_LIBPLACEBO:
            if (!shader_path) {
                fprintf(stderr, "Shader path must be provided for Libplacebo filter\n");
                ret = -1;
                goto end;
            }
            filter = new LibplaceboFilter(output_width, output_height, shader_path);
            break;
        case FILTER_REALESRGAN:
            filter = new RealesrganFilter();
            break;
        default:
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
