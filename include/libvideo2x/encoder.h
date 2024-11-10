#ifndef ENCODER_H
#define ENCODER_H

#include <filesystem>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

#include "libvideo2x.h"

int init_encoder(
    AVBufferRef *hw_ctx,
    std::filesystem::path out_fpath,
    AVFormatContext *ifmt_ctx,
    AVFormatContext **ofmt_ctx,
    AVCodecContext **enc_ctx,
    AVCodecContext *dec_ctx,
    EncoderConfig *encoder_config,
    int in_vstream_idx,
    int *out_vstream_idx,
    int **stream_map
);

int write_frame(
    AVFrame *frame,
    AVCodecContext *enc_ctx,
    AVFormatContext *ofmt_ctx,
    int out_vstream_idx,
    int64_t frame_idx
);

int flush_encoder(AVCodecContext *enc_ctx, AVFormatContext *ofmt_ctx, int out_vstream_idx);

#endif  // ENCODER_H
