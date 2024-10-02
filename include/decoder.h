#ifndef DECODER_H
#define DECODER_H

#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>

int init_decoder(
    const char *input_filename,
    AVFormatContext **fmt_ctx,
    AVCodecContext **dec_ctx,
    int *video_stream_index
);

#endif  // DECODER_H
