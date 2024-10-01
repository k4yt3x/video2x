#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#include "encoder.h"

int init_decoder(
    const char *input_filename,
    AVFormatContext **fmt_ctx,
    AVCodecContext **dec_ctx,
    int *video_stream_index
) {
    AVFormatContext *ifmt_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    int ret;

    if ((ret = avformat_open_input(&ifmt_ctx, input_filename, NULL, NULL)) < 0) {
        fprintf(stderr, "Could not open input file '%s'\n", input_filename);
        return ret;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        fprintf(stderr, "Failed to retrieve input stream information\n");
        return ret;
    }

    // Find the first video stream
    ret = av_find_best_stream(ifmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find video stream in the input, aborting\n");
        return ret;
    }

    int stream_index = ret;
    AVStream *video_stream = ifmt_ctx->streams[stream_index];

    // Set up the decoder
    const AVCodec *dec = avcodec_find_decoder(video_stream->codecpar->codec_id);
    if (!dec) {
        fprintf(stderr, "Failed to find decoder for stream #%u\n", stream_index);
        return AVERROR_DECODER_NOT_FOUND;
    }

    codec_ctx = avcodec_alloc_context3(dec);
    if (!codec_ctx) {
        fprintf(stderr, "Failed to allocate the decoder context\n");
        return AVERROR(ENOMEM);
    }

    if ((ret = avcodec_parameters_to_context(codec_ctx, video_stream->codecpar)) < 0) {
        fprintf(stderr, "Failed to copy decoder parameters to input decoder context\n");
        return ret;
    }

    // Set decoder time base and frame rate
    codec_ctx->time_base = video_stream->time_base;
    codec_ctx->pkt_timebase = video_stream->time_base;
    AVRational frame_rate = av_guess_frame_rate(ifmt_ctx, video_stream, NULL);
    codec_ctx->framerate = frame_rate;

    if ((ret = avcodec_open2(codec_ctx, dec, NULL)) < 0) {
        fprintf(stderr, "Failed to open decoder for stream #%u\n", stream_index);
        return ret;
    }

    *fmt_ctx = ifmt_ctx;
    *dec_ctx = codec_ctx;
    *video_stream_index = stream_index;

    return 0;
}

int flush_decoder(
    AVCodecContext *dec_ctx,
    AVFilterContext *buffersrc_ctx,
    AVFilterContext *buffersink_ctx,
    AVCodecContext *enc_ctx,
    AVFormatContext *ofmt_ctx
) {
    int ret;
    AVFrame *frame = av_frame_alloc();
    AVFrame *filt_frame = av_frame_alloc();

    if (!frame || !filt_frame) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    ret = avcodec_send_packet(dec_ctx, NULL);
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
            break;
        } else if (ret < 0) {
            fprintf(stderr, "Error during decoding\n");
            goto end;
        }

        if (av_buffersrc_add_frame(buffersrc_ctx, frame) < 0) {
            fprintf(stderr, "Error while feeding the filter graph\n");
            break;
        }

        while (1) {
            ret = av_buffersink_get_frame(buffersink_ctx, filt_frame);
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
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

    // Flush the encoder
    ret = flush_encoder(enc_ctx, ofmt_ctx);
    if (ret < 0) {
        goto end;
    }

end:
    av_frame_free(&frame);
    av_frame_free(&filt_frame);
    return ret;
}
