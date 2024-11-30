#ifndef AVUTILS_H
#define AVUTILS_H

extern "C" {
#include <libavformat/avformat.h>
}

#define CALC_FFMPEG_VERSION(a, b, c) (a << 16 | b << 8 | c)

AVRational get_video_frame_rate(AVFormatContext *ifmt_ctx, int in_vstream_idx);

int64_t get_video_frame_count(AVFormatContext *ifmt_ctx, int in_vstream_idx);

enum AVPixelFormat
get_encoder_default_pix_fmt(const AVCodec *encoder, AVPixelFormat target_pix_fmt);

float get_frame_diff(AVFrame *frame1, AVFrame *frame2);

#endif  // AVUTILS_H
