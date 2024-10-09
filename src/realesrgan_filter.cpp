#include "realesrgan_filter.h"

#include <cstdint>
#include <cstdio>
#include <string>

#include "conversions.h"
#include "fsutils.h"

RealesrganFilter::RealesrganFilter(
    int gpuid,
    bool tta_mode,
    int scaling_factor,
    const char *model,
    const std::filesystem::path custom_model_param_path,
    const std::filesystem::path custom_model_bin_path
)
    : realesrgan(nullptr),
      gpuid(gpuid),
      tta_mode(tta_mode),
      scaling_factor(scaling_factor),
      model(model),
      custom_model_param_path(std::move(custom_model_param_path)),
      custom_model_bin_path(std::move(custom_model_bin_path)) {}

RealesrganFilter::~RealesrganFilter() {
    if (realesrgan) {
        delete realesrgan;
        realesrgan = nullptr;
    }
}

int RealesrganFilter::init(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx, AVBufferRef *hw_ctx) {
    // Construct the model paths using std::filesystem
    std::filesystem::path model_param_path;
    std::filesystem::path model_bin_path;

    if (model) {
        // Find the model paths by model name if provided
        model_param_path = std::filesystem::path("models") /
                           (std::string(model) + "-x" + std::to_string(scaling_factor) + ".param");
        model_bin_path = std::filesystem::path("models") /
                         (std::string(model) + "-x" + std::to_string(scaling_factor) + ".bin");
    } else if (!custom_model_param_path.empty() && !custom_model_bin_path.empty()) {
        // Use the custom model paths if provided
        model_param_path = custom_model_param_path;
        model_bin_path = custom_model_bin_path;
    } else {
        // Neither model name nor custom model paths provided
        fprintf(stderr, "Model or model paths must be provided for RealESRGAN filter\n");
        return -1;
    }

    // Get the full paths using a function that possibly modifies or validates the path
    std::filesystem::path model_param_full_path = find_resource_file(model_param_path);
    std::filesystem::path model_bin_full_path = find_resource_file(model_bin_path);

    // Check if the model files exist
    if (!std::filesystem::exists(model_param_full_path)) {
        fprintf(
            stderr, "RealESRGAN model param file not found: %s\n", model_param_full_path.c_str()
        );
        return -1;
    }
    if (!std::filesystem::exists(model_bin_full_path)) {
        fprintf(stderr, "RealESRGAN model bin file not found: %s\n", model_bin_full_path.c_str());
        return -1;
    }

    // Create a new RealESRGAN instance
    realesrgan = new RealESRGAN(gpuid, tta_mode);

    // Store the time bases
    input_time_base = dec_ctx->time_base;
    output_time_base = enc_ctx->time_base;
    output_pix_fmt = enc_ctx->pix_fmt;

    // Load the model
    if (realesrgan->load(model_param_full_path, model_bin_full_path) != 0) {
        fprintf(stderr, "Failed to load RealESRGAN model\n");
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

int RealesrganFilter::process_frame(AVFrame *input_frame, AVFrame **output_frame) {
    int ret;

    // Convert the input frame to RGB24
    ncnn::Mat input_mat = avframe_to_ncnn_mat(input_frame);
    if (input_mat.empty()) {
        fprintf(stderr, "Failed to convert AVFrame to ncnn::Mat\n");
        return -1;
    }

    // Allocate space for ouptut ncnn::Mat
    int output_width = input_mat.w * realesrgan->scale;
    int output_height = input_mat.h * realesrgan->scale;
    ncnn::Mat output_mat = ncnn::Mat(output_width, output_height, (size_t)3, 3);

    ret = realesrgan->process(input_mat, output_mat);
    if (ret != 0) {
        fprintf(stderr, "RealESRGAN processing failed\n");
        return ret;
    }

    // Convert ncnn::Mat to AVFrame
    *output_frame = ncnn_mat_to_avframe(output_mat, output_pix_fmt);

    // Rescale PTS to encoder's time base
    (*output_frame)->pts = av_rescale_q(input_frame->pts, input_time_base, output_time_base);

    // Return the processed frame to the caller
    return ret;
}

int RealesrganFilter::flush(std::vector<AVFrame *> &processed_frames) {
    // No special flushing needed for RealESRGAN
    return 0;
}
