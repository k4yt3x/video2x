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
#include <libavutil/buffer.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
}

#include "conversions.h"
#include "libvideo2x.h"

static enum AVPixelFormat get_encoder_default_pix_fmt(const AVCodec *encoder) {
    const enum AVPixelFormat *p = encoder->pix_fmts;
    if (!p) {
        fprintf(stderr, "No pixel formats supported by encoder\n");
        return AV_PIX_FMT_NONE;
    }
    return *p;
}

int init_encoder(
    AVBufferRef *hw_ctx,
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
    const AVCodec *encoder = avcodec_find_encoder(encoder_config->codec);
    if (!encoder) {
        fprintf(stderr, "Necessary encoder not found\n");
        return AVERROR_ENCODER_NOT_FOUND;
    }

    AVStream *out_stream = avformat_new_stream(fmt_ctx, NULL);
    if (!out_stream) {
        fprintf(stderr, "Failed allocating output stream\n");
        return AVERROR_UNKNOWN;
    }

    codec_ctx = avcodec_alloc_context3(encoder);
    if (!codec_ctx) {
        fprintf(stderr, "Failed to allocate the encoder context\n");
        return AVERROR(ENOMEM);
    }

    // Set hardware device context
    if (hw_ctx != nullptr) {
        codec_ctx->hw_device_ctx = av_buffer_ref(hw_ctx);
    }

    // Set encoding parameters
    codec_ctx->height = encoder_config->output_height;
    codec_ctx->width = encoder_config->output_width;
    codec_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
    codec_ctx->time_base = av_inv_q(dec_ctx->framerate);

    if (encoder_config->pix_fmt != AV_PIX_FMT_NONE) {
        // Use the specified pixel format
        codec_ctx->pix_fmt = encoder_config->pix_fmt;
    } else {
        // Fall back to the default pixel format
        codec_ctx->pix_fmt = get_encoder_default_pix_fmt(encoder);
        if (codec_ctx->pix_fmt == AV_PIX_FMT_NONE) {
            fprintf(stderr, "Could not get the default pixel format for the encoder\n");
            return AVERROR(EINVAL);
        }
    }

    if (codec_ctx->time_base.num == 0 || codec_ctx->time_base.den == 0) {
        codec_ctx->time_base = av_inv_q(av_guess_frame_rate(fmt_ctx, out_stream, NULL));
    }

    // Set the bit rate and other encoder parameters
    codec_ctx->bit_rate = encoder_config->bit_rate;

    char crf_str[16];
    snprintf(crf_str, sizeof(crf_str), "%.f", encoder_config->crf);

    // Set the CRF and preset for any codecs that support it
    av_opt_set(codec_ctx->priv_data, "crf", crf_str, 0);
    av_opt_set(codec_ctx->priv_data, "preset", encoder_config->preset, 0);

    if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if ((ret = avcodec_open2(codec_ctx, encoder, NULL)) < 0) {
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

    // Convert the frame to the encoder's pixel format if needed
    if (frame->format != enc_ctx->pix_fmt) {
        AVFrame *converted_frame = convert_avframe_pix_fmt(frame, enc_ctx->pix_fmt);
        if (!converted_frame) {
            fprintf(stderr, "Error converting frame to encoder's pixel format\n");
            return AVERROR_EXTERNAL;
        }

        converted_frame->pts = frame->pts;
        frame = converted_frame;
    }

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
