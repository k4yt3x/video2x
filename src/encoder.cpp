#include "encoder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <spdlog/spdlog.h>

#include "conversions.h"

static enum AVPixelFormat get_encoder_default_pix_fmt(const AVCodec *encoder) {
    const enum AVPixelFormat *p = encoder->pix_fmts;
    if (!p) {
        spdlog::error("No pixel formats supported by encoder");
        return AV_PIX_FMT_NONE;
    }
    return *p;
}

int init_encoder(
    AVBufferRef *hw_ctx,
    const char *out_fname,
    AVFormatContext *ifmt_ctx,
    AVFormatContext **ofmt_ctx,
    AVCodecContext **enc_ctx,
    AVCodecContext *dec_ctx,
    EncoderConfig *encoder_config,
    int vstream_idx,
    int **stream_map
) {
    AVFormatContext *fmt_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    int stream_index = 0;
    int ret;

    avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, out_fname);
    if (!fmt_ctx) {
        spdlog::error("Could not create output context");
        return AVERROR_UNKNOWN;
    }

    const AVCodec *encoder = avcodec_find_encoder(encoder_config->codec);
    if (!encoder) {
        spdlog::error(
            "Required video encoder not found for vcodec {}",
            avcodec_get_name(encoder_config->codec)
        );
        return AVERROR_ENCODER_NOT_FOUND;
    }

    // Create a new video stream in the output file
    AVStream *out_stream = avformat_new_stream(fmt_ctx, NULL);
    if (!out_stream) {
        spdlog::error("Failed to allocate the output video stream");
        return AVERROR_UNKNOWN;
    }

    codec_ctx = avcodec_alloc_context3(encoder);
    if (!codec_ctx) {
        spdlog::error("Failed to allocate the encoder context");
        return AVERROR(ENOMEM);
    }

    // Set hardware device context
    if (hw_ctx != nullptr) {
        codec_ctx->hw_device_ctx = av_buffer_ref(hw_ctx);
    }

    // Set encoding parameters
    codec_ctx->height = encoder_config->out_height;
    codec_ctx->width = encoder_config->out_width;
    codec_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
    codec_ctx->bit_rate = encoder_config->bit_rate;

    // Set the pixel format
    if (encoder_config->pix_fmt != AV_PIX_FMT_NONE) {
        // Use the specified pixel format
        codec_ctx->pix_fmt = encoder_config->pix_fmt;
    } else {
        // Fall back to the default pixel format
        codec_ctx->pix_fmt = get_encoder_default_pix_fmt(encoder);
        if (codec_ctx->pix_fmt == AV_PIX_FMT_NONE) {
            spdlog::error("Could not get the default pixel format for the encoder");
            return AVERROR(EINVAL);
        }
    }

    // Set the output video's time base
    if (dec_ctx->time_base.num > 0 && dec_ctx->time_base.den > 0) {
        codec_ctx->time_base = dec_ctx->time_base;
    } else {
        codec_ctx->time_base = av_inv_q(av_guess_frame_rate(ifmt_ctx, out_stream, NULL));
    }

    // Set the output video's frame rate
    if (dec_ctx->framerate.num > 0 && dec_ctx->framerate.den > 0) {
        codec_ctx->framerate = dec_ctx->framerate;
    } else {
        codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx, out_stream, NULL);
    }

    // Set the CRF and preset for any codecs that support it
    std::string crf_str = std::to_string(encoder_config->crf);
    av_opt_set(codec_ctx->priv_data, "crf", crf_str.c_str(), 0);
    av_opt_set(codec_ctx->priv_data, "preset", encoder_config->preset, 0);

    if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if ((ret = avcodec_open2(codec_ctx, encoder, NULL)) < 0) {
        spdlog::error("Cannot open video encoder");
        return ret;
    }

    ret = avcodec_parameters_from_context(out_stream->codecpar, codec_ctx);
    if (ret < 0) {
        spdlog::error("Failed to copy encoder parameters to output video stream");
        return ret;
    }

    out_stream->time_base = codec_ctx->time_base;
    out_stream->avg_frame_rate = codec_ctx->framerate;
    out_stream->r_frame_rate = codec_ctx->framerate;

    if (encoder_config->copy_streams) {
        // Allocate the stream map
        *stream_map =
            reinterpret_cast<int *>(av_malloc_array(ifmt_ctx->nb_streams, sizeof(**stream_map)));
        if (!*stream_map) {
            spdlog::error("Could not allocate stream mapping");
            return AVERROR(ENOMEM);
        }

        // Map the video stream
        (*stream_map)[vstream_idx] = stream_index++;

        // Loop through each stream in the input file
        for (int i = 0; i < static_cast<int>(ifmt_ctx->nb_streams); i++) {
            AVStream *in_stream = ifmt_ctx->streams[i];
            AVCodecParameters *in_codecpar = in_stream->codecpar;

            if (i == vstream_idx) {
                // Video stream is already handled
                continue;
            }

            if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
                in_codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
                (*stream_map)[i] = -1;
                continue;
            }

            // Create corresponding output stream
            AVStream *out_copied_stream = avformat_new_stream(fmt_ctx, NULL);
            if (!out_copied_stream) {
                spdlog::error("Failed allocating output stream");
                return AVERROR_UNKNOWN;
            }

            ret = avcodec_parameters_copy(out_copied_stream->codecpar, in_codecpar);
            if (ret < 0) {
                spdlog::error("Failed to copy codec parameters");
                return ret;
            }
            out_copied_stream->codecpar->codec_tag = 0;

            // Copy time base
            out_copied_stream->time_base = in_stream->time_base;

            (*stream_map)[i] = stream_index++;
        }
    }

    // Open the output file
    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&fmt_ctx->pb, out_fname, AVIO_FLAG_WRITE);
        if (ret < 0) {
            spdlog::error("Could not open output file '{}'", out_fname);
            return ret;
        }
    }

    *ofmt_ctx = fmt_ctx;
    *enc_ctx = codec_ctx;

    return 0;
}

int write_frame(
    AVFrame *frame,
    AVCodecContext *enc_ctx,
    AVFormatContext *ofmt_ctx,
    int vstream_idx
) {
    AVFrame *converted_frame = nullptr;
    int ret;

    // Convert the frame to the encoder's pixel format if needed
    if (frame->format != enc_ctx->pix_fmt) {
        converted_frame = convert_avframe_pix_fmt(frame, enc_ctx->pix_fmt);
        if (!converted_frame) {
            spdlog::error("Error converting frame to encoder's pixel format");
            return AVERROR_EXTERNAL;
        }

        converted_frame->pts = frame->pts;
    }

    AVPacket *enc_pkt = av_packet_alloc();
    if (!enc_pkt) {
        spdlog::error("Could not allocate AVPacket");
        return AVERROR(ENOMEM);
    }

    if (converted_frame != nullptr) {
        ret = avcodec_send_frame(enc_ctx, converted_frame);
        av_frame_free(&converted_frame);
    } else {
        ret = avcodec_send_frame(enc_ctx, frame);
    }
    if (ret < 0) {
        spdlog::error("Error sending frame to encoder");
        av_packet_free(&enc_pkt);
        return ret;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(enc_ctx, enc_pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_packet_unref(enc_pkt);
            break;
        } else if (ret < 0) {
            spdlog::error("Error encoding frame");
            av_packet_free(&enc_pkt);
            return ret;
        }

        // Rescale packet timestamps
        av_packet_rescale_ts(
            enc_pkt, enc_ctx->time_base, ofmt_ctx->streams[vstream_idx]->time_base
        );
        enc_pkt->stream_index = vstream_idx;

        // Write the packet
        ret = av_interleaved_write_frame(ofmt_ctx, enc_pkt);
        av_packet_unref(enc_pkt);
        if (ret < 0) {
            spdlog::error("Error muxing packet");
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
        spdlog::error("Could not allocate AVPacket");
        return AVERROR(ENOMEM);
    }

    ret = avcodec_send_frame(enc_ctx, NULL);
    while (ret >= 0) {
        ret = avcodec_receive_packet(enc_ctx, enc_pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_packet_unref(enc_pkt);
            break;
        } else if (ret < 0) {
            spdlog::error("Error encoding frame");
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
            spdlog::error("Error muxing packet");
            av_packet_free(&enc_pkt);
            return ret;
        }
    }

    av_packet_free(&enc_pkt);
    return 0;
}
