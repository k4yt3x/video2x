#ifndef ENCODER_H
#define ENCODER_H

#include <libavformat/avformat.h>

#include "libvideo2x.h"

int init_encoder(
    AVBufferRef *hw_ctx,
    const char *output_filename,
    AVFormatContext **ofmt_ctx,
    AVCodecContext **enc_ctx,
    AVCodecContext *dec_ctx,
    EncoderConfig *encoder_config
);

int encode_and_write_frame(AVFrame *frame, AVCodecContext *enc_ctx, AVFormatContext *ofmt_ctx);

int flush_encoder(AVCodecContext *enc_ctx, AVFormatContext *ofmt_ctx);

#endif  // ENCODER_H
