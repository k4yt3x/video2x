#include <stdio.h>
#include <stdlib.h>

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
#include "placebo.h"

int process_frames(
    AVFormatContext *fmt_ctx,
    AVFormatContext *ofmt_ctx,
    AVCodecContext *dec_ctx,
    AVCodecContext *enc_ctx,
    AVFilterContext *buffersrc_ctx,
    AVFilterContext *buffersink_ctx,
    int video_stream_index
) {
    int ret;
    AVPacket packet;
    AVFrame *frame = av_frame_alloc();
    AVFrame *filt_frame = av_frame_alloc();

    if (!frame || !filt_frame) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    while (1) {
        ret = av_read_frame(fmt_ctx, &packet);
        if (ret < 0) {
            break;
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

                // Set the frame PTS
                if (frame->pts == AV_NOPTS_VALUE) {
                    frame->pts = frame->best_effort_timestamp;
                }

                // Feed the frame to the filter graph
                if (av_buffersrc_add_frame(buffersrc_ctx, frame) < 0) {
                    fprintf(stderr, "Error while feeding the filter graph\n");
                    break;
                }

                // Get the filtered frame
                while (1) {
                    ret = av_buffersink_get_frame(buffersink_ctx, filt_frame);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                        break;
                    }
                    if (ret < 0) {
                        goto end;
                    }

                    filt_frame->pict_type = AV_PICTURE_TYPE_NONE;
                    // Rescale PTS to encoder's time base
                    filt_frame->pts = av_rescale_q(
                        filt_frame->pts, buffersink_ctx->inputs[0]->time_base, enc_ctx->time_base
                    );

                    // Encode the filtered frame
                    if ((ret = encode_and_write_frame(filt_frame, enc_ctx, ofmt_ctx)) < 0) {
                        goto end;
                    }

                    av_frame_unref(filt_frame);
                }
                av_frame_unref(frame);
            }
        }
        av_packet_unref(&packet);
    }

    // Flush the decoder and encoder
    ret = flush_decoder(dec_ctx, buffersrc_ctx, buffersink_ctx, enc_ctx, ofmt_ctx);
    if (ret < 0) {
        goto end;
    }

end:
    av_frame_free(&frame);
    av_frame_free(&filt_frame);
    return ret;
}

int process_video(
    const char *input_filename,
    const char *output_filename,
    const char *shader_path,
    int output_width,
    int output_height
) {
    AVFormatContext *fmt_ctx = NULL;
    AVFormatContext *ofmt_ctx = NULL;
    AVCodecContext *dec_ctx = NULL;
    AVCodecContext *enc_ctx = NULL;
    AVFilterGraph *filter_graph = NULL;
    AVFilterContext *buffersrc_ctx = NULL;
    AVFilterContext *buffersink_ctx = NULL;
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

    // Initialize libplacebo filter
    if ((ret = init_libplacebo(
             &filter_graph,
             &buffersrc_ctx,
             &buffersink_ctx,
             dec_ctx,
             output_width,
             output_height,
             shader_path
         )) < 0) {
        goto end;
    }

    // Process frames
    if ((ret = process_frames(
             fmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, buffersrc_ctx, buffersink_ctx, video_stream_index
         )) < 0) {
        goto end;
    }

    // Write trailer
    av_write_trailer(ofmt_ctx);

end:
    if (filter_graph) {
        avfilter_graph_free(&filter_graph);
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
