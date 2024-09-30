#ifndef ENCODER_H
#define ENCODER_H

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

int init_encoder(
    const char *output_filename,
    AVFormatContext **ofmt_ctx,
    AVCodecContext **enc_ctx,
    AVCodecContext *dec_ctx,
    int output_width,
    int output_height
);

int encode_and_write_frame(AVFrame *frame, AVCodecContext *enc_ctx, AVFormatContext *ofmt_ctx);

int flush_encoder(AVCodecContext *enc_ctx, AVFormatContext *ofmt_ctx);

#endif  // ENCODER_H
