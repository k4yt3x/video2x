#include "interpolator_rife.h"

#include <cstdio>
#include <filesystem>

#include <spdlog/spdlog.h>

#include "conversions.h"
#include "fsutils.h"

InterpolatorRIFE::InterpolatorRIFE(
    int gpuid,
    bool tta_mode,
    bool tta_temporal_mode,
    bool uhd_mode,
    int num_threads,
    bool rife_v2,
    bool rife_v4,
    const StringType model_name
)
    : rife_(nullptr),
      gpuid_(gpuid),
      tta_mode_(tta_mode),
      tta_temporal_mode_(tta_temporal_mode),
      uhd_mode_(uhd_mode),
      num_threads_(num_threads),
      rife_v2_(rife_v2),
      rife_v4_(rife_v4),
      model_name_(std::move(model_name)) {}

InterpolatorRIFE::~InterpolatorRIFE() {
    if (rife_) {
        delete rife_;
        rife_ = nullptr;
    }
}

int InterpolatorRIFE::init(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx, AVBufferRef *_) {
    // Construct the model directory path using std::filesystem
    std::filesystem::path model_param_dir;

    // Find the model paths by model name if provided
    model_param_dir = std::filesystem::path(STR("models")) / STR("rife") / model_name_;

    // Get the full paths using a function that possibly modifies or validates the path
    std::filesystem::path model_param_full_path = find_resource_file(model_param_dir);

    // Check if the model files exist
    if (!std::filesystem::exists(model_param_full_path)) {
        spdlog::error("RIFE model param directory not found: {}", model_param_dir.u8string());
        return -1;
    }

    // Create a new RIFE instance
    rife_ = new RIFE(
        gpuid_, tta_mode_, tta_temporal_mode_, uhd_mode_, num_threads_, rife_v2_, rife_v4_
    );

    // Store the time bases
    in_time_base_ = dec_ctx->time_base;
    out_time_base_ = enc_ctx->time_base;
    out_pix_fmt_ = enc_ctx->pix_fmt;

    // Load the model
    if (rife_->load(model_param_full_path) != 0) {
        spdlog::error("Failed to load RIFE model");
        return -1;
    }

    return 0;
}

int InterpolatorRIFE::interpolate(
    AVFrame *prev_frame,
    AVFrame *in_frame,
    AVFrame **out_frame,
    float time_step
) {
    int ret;

    /*
    ncnn::Mat in_mat1 = avframe_to_ncnn_mat(prev_frame);
    if (in_mat1.empty()) {
        spdlog::error("Failed to convert AVFrame to ncnn::Mat");
        return -1;
    }
    */

    // Convert the input frame to RGB24
    ncnn::Mat in_mat2 = avframe_to_ncnn_mat(in_frame);
    if (in_mat2.empty()) {
        spdlog::error("Failed to convert AVFrame to ncnn::Mat");
        return -1;
    }

    // Allocate space for output ncnn::Mat
    ncnn::Mat out_mat = ncnn::Mat(in_mat2.w, in_mat2.h, static_cast<size_t>(3), 3);

    // TODO: handle frames properly
    // ret = rife_->process(in_mat1, in_mat2, time_step, out_mat);
    ret = rife_->process(in_mat2, in_mat2, time_step, out_mat);
    if (ret != 0) {
        spdlog::error("RIFE processing failed");
        return ret;
    }

    // Convert ncnn::Mat to AVFrame
    *out_frame = ncnn_mat_to_avframe(out_mat, out_pix_fmt_);

    // Rescale PTS to encoder's time base
    (*out_frame)->pts = av_rescale_q(in_frame->pts, in_time_base_, out_time_base_);

    // Return the processed frame to the caller
    return ret;
}
