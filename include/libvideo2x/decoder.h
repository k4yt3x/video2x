#ifndef DECODER_H
#define DECODER_H

#include <filesystem>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

int init_decoder(
    AVHWDeviceType hw_type,
    AVBufferRef *hw_ctx,
    std::filesystem::path in_fpath,
    AVFormatContext **fmt_ctx,
    AVCodecContext **dec_ctx,
    int *in_vstream_idx
);

#endif  // DECODER_H
