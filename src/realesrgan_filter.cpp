#include <linux/limits.h>
#include <cstdint>
#include <cstdio>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
}

#include "conversions.h"
#include "fsutils.h"
#include "realesrgan.h"
#include "realesrgan_filter.h"

RealesrganFilter::RealesrganFilter(
    int gpuid,
    bool tta_mode,
    int scaling_factor,
    const char *model,
    const char *custom_model_param_path,
    const char *custom_model_bin_path
)
    : realesrgan(nullptr),
      gpuid(gpuid),
      tta_mode(tta_mode),
      scaling_factor(scaling_factor),
      model(model),
      custom_model_param_path(custom_model_param_path),
      custom_model_bin_path(custom_model_bin_path) {}

RealesrganFilter::~RealesrganFilter() {
    if (realesrgan) {
        delete realesrgan;
        realesrgan = nullptr;
    }
}

int RealesrganFilter::init(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx) {
    // Construct the model paths
    char model_param_path[PATH_MAX] = {0};
    char model_bin_path[PATH_MAX] = {0};

    if (model) {
        // Find the model paths by model name if provided
        snprintf(model_param_path, PATH_MAX, "models/%s-x%d.param", model, scaling_factor);
        snprintf(model_bin_path, PATH_MAX, "models/%s-x%d.bin", model, scaling_factor);

    } else if (custom_model_param_path && custom_model_bin_path) {
        // Use the custom model paths if provided
        snprintf(model_param_path, PATH_MAX, "%s", custom_model_param_path);
        snprintf(model_bin_path, PATH_MAX, "%s", custom_model_bin_path);

    } else {
        // Neither model name nor custom model paths provided
        fprintf(stderr, "Model or model paths must be provided for RealESRGAN filter\n");
        return -1;
    }

    // Get the full paths
    path_t model_param_full_path = get_full_path(model_param_path);
    path_t model_bin_full_path = get_full_path(model_bin_path);

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

AVFrame *RealesrganFilter::process_frame(AVFrame *input_frame) {
    // Convert the input frame to RGB24
    ncnn::Mat input_mat = avframe_to_ncnn_mat(input_frame);
    if (input_mat.empty()) {
        fprintf(stderr, "Failed to convert AVFrame to ncnn::Mat\n");
        return nullptr;
    }

    // Allocate space for ouptut ncnn::Mat
    int output_width = input_mat.w * realesrgan->scale;
    int output_height = input_mat.h * realesrgan->scale;
    ncnn::Mat output_mat = ncnn::Mat(output_width, output_height, (size_t)3, 3);

    if (realesrgan->process(input_mat, output_mat) != 0) {
        fprintf(stderr, "RealESRGAN processing failed\n");
        return nullptr;
    }

    // Convert ncnn::Mat to AVFrame
    AVFrame *output_frame = ncnn_mat_to_avframe(output_mat, output_pix_fmt);

    // Rescale PTS to encoder's time base
    output_frame->pts = av_rescale_q(input_frame->pts, input_time_base, output_time_base);

    // Return the processed frame to the caller
    return output_frame;
}

int RealesrganFilter::flush(std::vector<AVFrame *> &processed_frames) {
    // No special flushing needed for RealESRGAN
    return 0;
}
