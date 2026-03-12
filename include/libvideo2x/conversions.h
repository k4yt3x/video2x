#pragma once

extern "C" {
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
}

#include <mat.h>

namespace video2x {
namespace conversions {

// Convert AVFrame to another pixel format
AVFrame* convert_avframe_pix_fmt(AVFrame* src_frame, AVPixelFormat pix_fmt);

// Convert AVFrame to ncnn::Mat
// color_hint: optional frame whose color metadata is used when the source frame
// has AVCOL_SPC_UNSPECIFIED / AVCOL_RANGE_UNSPECIFIED (common for per-frame metadata).
ncnn::Mat avframe_to_ncnn_mat(AVFrame* frame, const AVFrame* color_hint = nullptr);

// Convert ncnn::Mat to AVFrame
// color_hint: optional frame whose color metadata is used to configure the
// YUV output color matrix and to populate the output frame's color properties.
AVFrame* ncnn_mat_to_avframe(
    const ncnn::Mat& mat,
    AVPixelFormat pix_fmt,
    const AVFrame* color_hint = nullptr
);

}  // namespace conversions
}  // namespace video2x
