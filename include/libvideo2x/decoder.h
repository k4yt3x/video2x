#ifndef DECODER_H
#define DECODER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

int init_decoder(
    AVHWDeviceType hw_type,
    AVBufferRef *hw_ctx,
    const char *in_fname,
    AVFormatContext **fmt_ctx,
    AVCodecContext **dec_ctx,
    int *vstream_idx
);

#endif  // DECODER_H
