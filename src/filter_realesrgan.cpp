#include "filter_realesrgan.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>

#include <spdlog/spdlog.h>

#include "conversions.h"
#include "fsutils.h"

namespace video2x {
namespace processors {

FilterRealesrgan::FilterRealesrgan(
    int gpuid,
    bool tta_mode,
    int scaling_factor,
    const fsutils::StringType model_name
)
    : realesrgan_(nullptr),
      gpuid_(gpuid),
      tta_mode_(tta_mode),
      scaling_factor_(scaling_factor),
      model_name_(std::move(model_name)) {}

FilterRealesrgan::~FilterRealesrgan() {
    if (realesrgan_) {
        delete realesrgan_;
        realesrgan_ = nullptr;
    }
}

int FilterRealesrgan::init(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx, AVBufferRef *) {
    // Construct the model paths using std::filesystem
    std::filesystem::path model_param_path;
    std::filesystem::path model_bin_path;

    fsutils::StringType param_file_name =
        model_name_ + STR("-x") + fsutils::to_string_type(scaling_factor_) + STR(".param");
    fsutils::StringType bin_file_name =
        model_name_ + STR("-x") + fsutils::to_string_type(scaling_factor_) + STR(".bin");

    // Find the model paths by model name if provided
    model_param_path = std::filesystem::path(STR("models")) / STR("realesrgan") / param_file_name;
    model_bin_path = std::filesystem::path(STR("models")) / STR("realesrgan") / bin_file_name;

    // Get the full paths using a function that possibly modifies or validates the path
    std::filesystem::path model_param_full_path =
        fsutils::find_resource_file(model_param_path);
    std::filesystem::path model_bin_full_path =
        fsutils::find_resource_file(model_bin_path);

    // Check if the model files exist
    if (!std::filesystem::exists(model_param_full_path)) {
        spdlog::error("RealESRGAN model param file not found: {}", model_param_path.u8string());
        return -1;
    }
    if (!std::filesystem::exists(model_bin_full_path)) {
        spdlog::error("RealESRGAN model bin file not found: {}", model_bin_path.u8string());
        return -1;
    }

    // Create a new RealESRGAN instance
    realesrgan_ = new RealESRGAN(gpuid_, tta_mode_);

    // Store the time bases
    in_time_base_ = dec_ctx->time_base;
    out_time_base_ = enc_ctx->time_base;
    out_pix_fmt_ = enc_ctx->pix_fmt;

    // Load the model
    if (realesrgan_->load(model_param_full_path, model_bin_full_path) != 0) {
        spdlog::error("Failed to load RealESRGAN model");
        return -1;
    }

    // Set RealESRGAN parameters
    realesrgan_->scale = scaling_factor_;
    realesrgan_->prepadding = 10;

    // Calculate tilesize based on GPU heap budget
    uint32_t heap_budget = ncnn::get_gpu_device(gpuid_)->get_heap_budget();
    if (heap_budget > 1900) {
        realesrgan_->tilesize = 200;
    } else if (heap_budget > 550) {
        realesrgan_->tilesize = 100;
    } else if (heap_budget > 190) {
        realesrgan_->tilesize = 64;
    } else {
        realesrgan_->tilesize = 32;
    }

    return 0;
}

int FilterRealesrgan::filter(AVFrame *in_frame, AVFrame **out_frame) {
    int ret;

    // Convert the input frame to RGB24
    ncnn::Mat in_mat = conversions::avframe_to_ncnn_mat(in_frame);
    if (in_mat.empty()) {
        spdlog::error("Failed to convert AVFrame to ncnn::Mat");
        return -1;
    }

    // Allocate space for output ncnn::Mat
    int output_width = in_mat.w * realesrgan_->scale;
    int output_height = in_mat.h * realesrgan_->scale;
    ncnn::Mat out_mat = ncnn::Mat(output_width, output_height, static_cast<size_t>(3), 3);

    ret = realesrgan_->process(in_mat, out_mat);
    if (ret != 0) {
        spdlog::error("RealESRGAN processing failed");
        return ret;
    }

    // Convert ncnn::Mat to AVFrame
    *out_frame = conversions::ncnn_mat_to_avframe(out_mat, out_pix_fmt_);

    // Rescale PTS to encoder's time base
    (*out_frame)->pts = av_rescale_q(in_frame->pts, in_time_base_, out_time_base_);

    // Return the processed frame to the caller
    return ret;
}

void FilterRealesrgan::get_output_dimensions(
    const ProcessorConfig &,
    int in_width,
    int in_height,
    int &out_width,
    int &out_height
) const {
    out_width = in_width * scaling_factor_;
    out_height = in_height * scaling_factor_;
}

}  // namespace processors
}  // namespace video2x
