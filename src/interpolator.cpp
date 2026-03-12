#include "processor.h"

#include <spdlog/spdlog.h>

#include "avutils.h"
#include "conversions.h"
#include "logger_manager.h"

namespace video2x {
namespace processors {

// Pass the frame through the same pixel-format round-trip used for interpolated
// frames so that original and interpolated frames have identical quantization
// characteristics, eliminating flickering during playback.
int Interpolator::normalize(AVFrame* in_frame, AVFrame** out_frame) {
    AVFrame color_hint = make_color_hint();

    ncnn::Mat mat = conversions::avframe_to_ncnn_mat(in_frame, &color_hint);
    if (mat.empty()) {
        logger()->error("Failed to convert AVFrame to ncnn::Mat during normalization");
        return -1;
    }

    AVFrame* normalized_frame_raw =
        conversions::ncnn_mat_to_avframe(mat, out_pix_fmt_, &color_hint);
    std::unique_ptr<AVFrame, decltype(&avutils::av_frame_deleter)> normalized_frame(
        normalized_frame_raw, &avutils::av_frame_deleter
    );
    if (normalized_frame == nullptr) {
        logger()->error("Failed to convert ncnn::Mat to AVFrame during normalization");
        return AVERROR(ENOMEM);
    }

    normalized_frame->pts = av_rescale_q(in_frame->pts, in_time_base_, out_time_base_);
    *out_frame = normalized_frame.release();
    return 0;
}

}  // namespace processors
}  // namespace video2x
