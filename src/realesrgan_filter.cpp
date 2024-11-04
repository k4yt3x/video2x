#include "realesrgan_filter.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>

#include <spdlog/spdlog.h>

#include "conversions.h"
#include "fsutils.h"

RealesrganFilter::RealesrganFilter(
    int gpuid,
    bool tta_mode,
    int scaling_factor,
    const StringType model_name
)
    : realesrgan(nullptr),
      gpuid(gpuid),
      tta_mode(tta_mode),
      scaling_factor(scaling_factor),
      model_name(std::move(model_name)) {}

RealesrganFilter::~RealesrganFilter() {
    if (realesrgan) {
        delete realesrgan;
        realesrgan = nullptr;
    }
}

int RealesrganFilter::init(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx, AVBufferRef *_) {
    // Construct the model paths using std::filesystem
    std::filesystem::path model_param_path;
    std::filesystem::path model_bin_path;

    StringType param_file_name =
        model_name + STR("-x") + to_string_type(scaling_factor) + STR(".param");
    StringType bin_file_name =
        model_name + STR("-x") + to_string_type(scaling_factor) + STR(".bin");

    // Find the model paths by model name if provided
    model_param_path = std::filesystem::path(STR("models")) / STR("realesrgan") / param_file_name;
    model_bin_path = std::filesystem::path(STR("models")) / STR("realesrgan") / bin_file_name;

    // Get the full paths using a function that possibly modifies or validates the path
    std::filesystem::path model_param_full_path = find_resource_file(model_param_path);
    std::filesystem::path model_bin_full_path = find_resource_file(model_bin_path);

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
    realesrgan = new RealESRGAN(gpuid, tta_mode);

    // Store the time bases
    in_time_base = dec_ctx->time_base;
    out_time_base = enc_ctx->time_base;
    out_pix_fmt = enc_ctx->pix_fmt;

    // Load the model
    if (realesrgan->load(model_param_full_path, model_bin_full_path) != 0) {
        spdlog::error("Failed to load RealESRGAN model");
        return -1;
    }

    // Set RealESRGAN parameters
    realesrgan->scale = scaling_factor;
    realesrgan->prepadding = 10;

    // Calculate tilesize based on GPU heap budget
    uint32_t heap_budget = ncnn::get_gpu_device(gpuid)->get_heap_budget();
    if (heap_budget > 1900) {
        realesrgan->tilesize = 200;
    } else if (heap_budget > 550) {
        realesrgan->tilesize = 100;
    } else if (heap_budget > 190) {
        realesrgan->tilesize = 64;
    } else {
        realesrgan->tilesize = 32;
    }

    return 0;
}

int RealesrganFilter::process_frame(AVFrame *in_frame, AVFrame **out_frame) {
    int ret;

    // Convert the input frame to RGB24
    ncnn::Mat in_mat = avframe_to_ncnn_mat(in_frame);
    if (in_mat.empty()) {
        spdlog::error("Failed to convert AVFrame to ncnn::Mat");
        return -1;
    }

    // Allocate space for ouptut ncnn::Mat
    int output_width = in_mat.w * realesrgan->scale;
    int output_height = in_mat.h * realesrgan->scale;
    ncnn::Mat out_mat = ncnn::Mat(output_width, output_height, static_cast<size_t>(3), 3);

    ret = realesrgan->process(in_mat, out_mat);
    if (ret != 0) {
        spdlog::error("RealESRGAN processing failed");
        return ret;
    }

    // Convert ncnn::Mat to AVFrame
    *out_frame = ncnn_mat_to_avframe(out_mat, out_pix_fmt);

    // Rescale PTS to encoder's time base
    (*out_frame)->pts = av_rescale_q(in_frame->pts, in_time_base, out_time_base);

    // Return the processed frame to the caller
    return ret;
}
