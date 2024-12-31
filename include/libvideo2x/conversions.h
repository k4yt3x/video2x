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
ncnn::Mat avframe_to_ncnn_mat(AVFrame* frame);

// Convert ncnn::Mat to AVFrame
AVFrame* ncnn_mat_to_avframe(const ncnn::Mat& mat, AVPixelFormat pix_fmt);

}  // namespace conversions
}  // namespace video2x
