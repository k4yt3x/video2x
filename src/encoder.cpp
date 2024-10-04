#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec.h>
#include <libavcodec/codec_id.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/rational.h>
}

#include "libvideo2x.h"

int init_encoder(
    const char *output_filename,
    AVFormatContext **ofmt_ctx,
    AVCodecContext **enc_ctx,
    AVCodecContext *dec_ctx,
    EncoderConfig *encoder_config
) {
    AVFormatContext *fmt_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    int ret;

    avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, output_filename);
    if (!fmt_ctx) {
        fprintf(stderr, "Could not create output context\n");
        return AVERROR_UNKNOWN;
    }

    // Create a new video stream
    const AVCodec *enc = avcodec_find_encoder(encoder_config->codec);
    if (!enc) {
        fprintf(stderr, "Necessary encoder not found\n");
        return AVERROR_ENCODER_NOT_FOUND;
    }

    AVStream *out_stream = avformat_new_stream(fmt_ctx, NULL);
    if (!out_stream) {
        fprintf(stderr, "Failed allocating output stream\n");
        return AVERROR_UNKNOWN;
    }

    codec_ctx = avcodec_alloc_context3(enc);
    if (!codec_ctx) {
        fprintf(stderr, "Failed to allocate the encoder context\n");
        return AVERROR(ENOMEM);
    }

    // Set encoding parameters
    codec_ctx->height = encoder_config->output_height;
    codec_ctx->width = encoder_config->output_width;
    codec_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
    codec_ctx->pix_fmt = encoder_config->pix_fmt;
    codec_ctx->time_base = av_inv_q(dec_ctx->framerate);

    if (codec_ctx->time_base.num == 0 || codec_ctx->time_base.den == 0) {
        codec_ctx->time_base = av_inv_q(av_guess_frame_rate(fmt_ctx, out_stream, NULL));
    }

    // Set the bit rate and other encoder parameters if needed
    codec_ctx->bit_rate = encoder_config->bit_rate;
    codec_ctx->gop_size = 60;     // Keyframe interval
    codec_ctx->max_b_frames = 3;  // B-frames
    codec_ctx->keyint_min = 60;   // Maximum GOP size

    char crf_str[16];
    snprintf(crf_str, sizeof(crf_str), "%.f", encoder_config->crf);
    if (encoder_config->codec == AV_CODEC_ID_H264 || encoder_config->codec == AV_CODEC_ID_HEVC) {
        av_opt_set(codec_ctx->priv_data, "crf", crf_str, 0);
        av_opt_set(codec_ctx->priv_data, "preset", encoder_config->preset, 0);
    }

    if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if ((ret = avcodec_open2(codec_ctx, enc, NULL)) < 0) {
        fprintf(stderr, "Cannot open video encoder\n");
        return ret;
    }

    ret = avcodec_parameters_from_context(out_stream->codecpar, codec_ctx);
    if (ret < 0) {
        fprintf(stderr, "Failed to copy encoder parameters to output stream\n");
        return ret;
    }

    out_stream->time_base = codec_ctx->time_base;

    // Open the output file
    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&fmt_ctx->pb, output_filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "Could not open output file '%s'\n", output_filename);
            return ret;
        }
    }

    *ofmt_ctx = fmt_ctx;
    *enc_ctx = codec_ctx;

    return 0;
}

int encode_and_write_frame(AVFrame *frame, AVCodecContext *enc_ctx, AVFormatContext *ofmt_ctx) {
    int ret;
    AVPacket *enc_pkt = av_packet_alloc();
    if (!enc_pkt) {
        fprintf(stderr, "Could not allocate AVPacket\n");
        return AVERROR(ENOMEM);
    }

    ret = avcodec_send_frame(enc_ctx, frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending frame to encoder\n");
        av_packet_free(&enc_pkt);
        return ret;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(enc_ctx, enc_pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_packet_unref(enc_pkt);
            break;
        } else if (ret < 0) {
            fprintf(stderr, "Error during encoding\n");
            av_packet_free(&enc_pkt);
            return ret;
        }

        // Rescale packet timestamps
        av_packet_rescale_ts(enc_pkt, enc_ctx->time_base, ofmt_ctx->streams[0]->time_base);
        enc_pkt->stream_index = ofmt_ctx->streams[0]->index;

        // Write the packet
        ret = av_interleaved_write_frame(ofmt_ctx, enc_pkt);
        av_packet_unref(enc_pkt);
        if (ret < 0) {
            fprintf(stderr, "Error muxing packet\n");
            av_packet_free(&enc_pkt);
            return ret;
        }
    }

    av_packet_free(&enc_pkt);
    return 0;
}

int flush_encoder(AVCodecContext *enc_ctx, AVFormatContext *ofmt_ctx) {
    int ret;
    AVPacket *enc_pkt = av_packet_alloc();
    if (!enc_pkt) {
        fprintf(stderr, "Could not allocate AVPacket\n");
        return AVERROR(ENOMEM);
    }

    ret = avcodec_send_frame(enc_ctx, NULL);
    while (ret >= 0) {
        ret = avcodec_receive_packet(enc_ctx, enc_pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_packet_unref(enc_pkt);
            break;
        } else if (ret < 0) {
            fprintf(stderr, "Error during encoding\n");
            av_packet_free(&enc_pkt);
            return ret;
        }

        // Rescale packet timestamps
        av_packet_rescale_ts(enc_pkt, enc_ctx->time_base, ofmt_ctx->streams[0]->time_base);
        enc_pkt->stream_index = ofmt_ctx->streams[0]->index;

        // Write the packet
        ret = av_interleaved_write_frame(ofmt_ctx, enc_pkt);
        av_packet_unref(enc_pkt);
        if (ret < 0) {
            fprintf(stderr, "Error muxing packet\n");
            av_packet_free(&enc_pkt);
            return ret;
        }
    }

    av_packet_free(&enc_pkt);
    return 0;
}
