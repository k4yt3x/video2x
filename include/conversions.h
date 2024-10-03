#ifndef CONVERSIONS_H
#define CONVERSIONS_H

#include <libavutil/frame.h>
#include <ncnn/mat.h>

ncnn::Mat avframe_to_ncnn_mat(AVFrame *frame);

AVFrame *convert_avframe_to_bgr24(AVFrame *src_frame);

AVFrame *ncnn_mat_to_avframe(const ncnn::Mat &mat, AVPixelFormat pix_fmt);

#endif  // CONVERSIONS_H
