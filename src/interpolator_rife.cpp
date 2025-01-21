#include "interpolator_rife.h"

#include <cstdio>
#include <filesystem>

#include <spdlog/spdlog.h>

#include "conversions.h"
#include "fsutils.h"
#include "logger_manager.h"

namespace video2x {
namespace processors {

InterpolatorRIFE::InterpolatorRIFE(
    int gpuid,
    bool tta_mode,
    bool tta_temporal_mode,
    bool uhd_mode,
    int num_threads,
    const fsutils::StringType model_name
)
    : rife_(nullptr),
      gpuid_(gpuid),
      tta_mode_(tta_mode),
      tta_temporal_mode_(tta_temporal_mode),
      uhd_mode_(uhd_mode),
      num_threads_(num_threads),
      model_name_(std::move(model_name)) {}

InterpolatorRIFE::~InterpolatorRIFE() {
    delete rife_;
    rife_ = nullptr;
}

int InterpolatorRIFE::init(AVCodecContext* dec_ctx, AVCodecContext* enc_ctx, AVBufferRef*) {
    // Construct the model directory path using std::filesystem
    std::filesystem::path model_dir =
        std::filesystem::path(STR("models")) / STR("rife") / model_name_;

    // Get the full paths using a function that possibly modifies or validates the path
    std::optional<std::filesystem::path> model_dir_full_path = fsutils::find_resource(model_dir);

    // Check if the model files exist
    if (!model_dir_full_path.has_value()) {
        logger()->error("RIFE model param directory not found: {}", model_dir.u8string());
        return -1;
    }

    // Automatically infer the RIFE model generation based on the model name
    bool rife_v2 = false;
    bool rife_v4 = false;
    int rife_padding = 32;
    if (model_name_.find(STR("rife-v2")) != fsutils::StringType::npos) {
        rife_v2 = true;
    } else if (model_name_.find(STR("rife-v3")) != fsutils::StringType::npos) {
        rife_v2 = true;
    } else if (model_name_.find(STR("rife-v4")) != fsutils::StringType::npos) {
        rife_v4 = true;
        if (model_name_.find(STR("rife-v4.25")) != fsutils::StringType::npos) {
            rife_padding = 64;
        }
        if (model_name_.find(STR("rife-v4.25-lite")) != fsutils::StringType::npos) {
            rife_padding = 128;
        }
        if (model_name_.find(STR("rife-v4.26")) != fsutils::StringType::npos) {
            rife_padding = 64;
        }
    } else if (model_name_.find(STR("rife")) == fsutils::StringType::npos) {
        logger()->critical("Failed to infer RIFE model generation from model name");
        return -1;
    }

    // Create a new RIFE instance
    rife_ = new RIFE(
        gpuid_,
        tta_mode_,
        tta_temporal_mode_,
        uhd_mode_,
        num_threads_,
        rife_v2,
        rife_v4,
        rife_padding
    );

    // Store the time bases
    in_time_base_ = dec_ctx->time_base;
    out_time_base_ = enc_ctx->time_base;
    out_pix_fmt_ = enc_ctx->pix_fmt;

    // Load the model
    if (rife_->load(model_dir_full_path.value()) != 0) {
        logger()->error("Failed to load RIFE model");
        return -1;
    }

    return 0;
}

int InterpolatorRIFE::interpolate(
    AVFrame* prev_frame,
    AVFrame* in_frame,
    AVFrame** out_frame,
    float time_step
) {
    int ret;

    ncnn::Mat in_mat1 = conversions::avframe_to_ncnn_mat(prev_frame);
    if (in_mat1.empty()) {
        logger()->error("Failed to convert AVFrame to ncnn::Mat");
        return -1;
    }

    ncnn::Mat in_mat2 = conversions::avframe_to_ncnn_mat(in_frame);
    if (in_mat2.empty()) {
        logger()->error("Failed to convert AVFrame to ncnn::Mat");
        return -1;
    }

    // Allocate space for output ncnn::Mat
    ncnn::Mat out_mat = ncnn::Mat(in_mat2.w, in_mat2.h, static_cast<size_t>(3), 3);

    ret = rife_->process(in_mat1, in_mat2, time_step, out_mat);
    if (ret != 0) {
        logger()->error("RIFE processing failed");
        return ret;
    }

    // Convert ncnn::Mat to AVFrame
    *out_frame = conversions::ncnn_mat_to_avframe(out_mat, out_pix_fmt_);

    // Rescale PTS to encoder's time base
    (*out_frame)->pts = av_rescale_q(in_frame->pts, in_time_base_, out_time_base_);

    // Return the processed frame to the caller
    return ret;
}

void InterpolatorRIFE::get_output_dimensions(
    const ProcessorConfig&,
    int in_width,
    int in_height,
    int& out_width,
    int& out_height
) const {
    out_width = in_width;
    out_height = in_height;
}

}  // namespace processors
}  // namespace video2x
