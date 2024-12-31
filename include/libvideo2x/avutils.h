#pragma once

extern "C" {
#include <libavformat/avformat.h>
}

namespace video2x {
namespace avutils {

AVRational get_video_frame_rate(AVFormatContext* ifmt_ctx, int in_vstream_idx);

int64_t get_video_frame_count(AVFormatContext* ifmt_ctx, int in_vstream_idx);

AVPixelFormat get_encoder_default_pix_fmt(const AVCodec* encoder, AVPixelFormat target_pix_fmt);

float get_frame_diff(AVFrame* frame1, AVFrame* frame2);

void av_bufferref_deleter(AVBufferRef* bufferref);

void av_frame_deleter(AVFrame* frame);

void av_packet_deleter(AVPacket* packet);

}  // namespace avutils
}  // namespace video2x
