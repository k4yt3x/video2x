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
