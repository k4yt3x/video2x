#ifndef DECODER_H
#define DECODER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

int init_decoder(
    AVHWDeviceType hw_type,
    AVBufferRef *hw_ctx,
    const char *input_filename,
    AVFormatContext **fmt_ctx,
    AVCodecContext **dec_ctx,
    int *video_stream_index
);

#endif  // DECODER_H
