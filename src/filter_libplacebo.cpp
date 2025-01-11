#include "filter_libplacebo.h"

#include <cstdio>

#include <spdlog/spdlog.h>

#include "fsutils.h"
#include "libplacebo.h"
#include "logger_manager.h"

namespace video2x {
namespace processors {

FilterLibplacebo::FilterLibplacebo(
    uint32_t vk_device_index,
    const std::filesystem::path& shader_path,
    int width,
    int height
)
    : filter_graph_(nullptr),
      buffersrc_ctx_(nullptr),
      buffersink_ctx_(nullptr),
      vk_device_index_(vk_device_index),
      shader_path_(std::move(shader_path)),
      width_(width),
      height_(height) {}

FilterLibplacebo::~FilterLibplacebo() {
    if (buffersrc_ctx_) {
        avfilter_free(buffersrc_ctx_);
        buffersrc_ctx_ = nullptr;
    }
    if (buffersink_ctx_) {
        avfilter_free(buffersink_ctx_);
        buffersink_ctx_ = nullptr;
    }
    if (filter_graph_) {
        avfilter_graph_free(&filter_graph_);
        filter_graph_ = nullptr;
    }
}

int FilterLibplacebo::init(AVCodecContext* dec_ctx, AVCodecContext* enc_ctx, AVBufferRef*) {
    // Construct the shader path
    std::optional<std::filesystem::path> shader_full_path = std::nullopt;
    if (fsutils::file_is_readable(shader_path_)) {
        // If the shader path is directly readable, use it
        shader_full_path = shader_path_;
    } else {
        // Construct the fallback path using std::filesystem
        shader_full_path = fsutils::find_resource(
            std::filesystem::path(STR("models")) / STR("libplacebo") /
            (fsutils::path_to_string_type(shader_path_) + STR(".glsl"))
        );
    }

    // Check if the shader file exists
    if (!shader_full_path.has_value()) {
        logger()->error("libplacebo shader file not found: '{}'", shader_path_.u8string());
        return -1;
    }

    // Save the output time base
    in_time_base_ = dec_ctx->time_base;
    out_time_base_ = enc_ctx->time_base;

    // Initialize the libplacebo filter
    int ret = init_libplacebo(
        &filter_graph_,
        &buffersrc_ctx_,
        &buffersink_ctx_,
        dec_ctx,
        width_,
        height_,
        vk_device_index_,
        shader_full_path.value()
    );

    // Set these resources to nullptr since they are already freed by `avfilter_graph_free`
    if (ret < 0) {
        buffersrc_ctx_ = nullptr;
        buffersink_ctx_ = nullptr;
        filter_graph_ = nullptr;
    }
    return ret;
}

int FilterLibplacebo::filter(AVFrame* in_frame, AVFrame** out_frame) {
    int ret;

    // Get the filtered frame
    *out_frame = av_frame_alloc();
    if (*out_frame == nullptr) {
        logger()->error("Failed to allocate output frame");
        return -1;
    }

    // Feed the frame to the filter graph
    ret = av_buffersrc_add_frame(buffersrc_ctx_, in_frame);
    if (ret < 0) {
        logger()->error("Error while feeding the filter graph");
        av_frame_free(out_frame);
        return ret;
    }

    ret = av_buffersink_get_frame(buffersink_ctx_, *out_frame);
    if (ret < 0) {
        av_frame_free(out_frame);
        return ret;
    }

    // Rescale PTS to encoder's time base
    (*out_frame)->pts = av_rescale_q((*out_frame)->pts, in_time_base_, out_time_base_);

    // Return the processed frame to the caller
    return 0;
}

int FilterLibplacebo::flush(std::vector<AVFrame*>& flushed_frames) {
    int ret = av_buffersrc_add_frame(buffersrc_ctx_, nullptr);
    if (ret < 0) {
        logger()->error("Error while flushing filter graph");
        return ret;
    }

    // Retrieve all remaining frames from the filter graph
    while (1) {
        AVFrame* filt_frame = av_frame_alloc();
        if (filt_frame == nullptr) {
            return AVERROR(ENOMEM);
        }

        ret = av_buffersink_get_frame(buffersink_ctx_, filt_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_frame_free(&filt_frame);
            break;
        }
        if (ret < 0) {
            av_frame_free(&filt_frame);
            return ret;
        }

        // Rescale PTS to encoder's time base
        filt_frame->pts = av_rescale_q(filt_frame->pts, in_time_base_, out_time_base_);

        // Add to processed frames
        flushed_frames.push_back(filt_frame);
    }

    return 0;
}

void FilterLibplacebo::get_output_dimensions(
    const ProcessorConfig& proc_cfg,
    int,
    int,
    int& out_width,
    int& out_height
) const {
    out_width = proc_cfg.width;
    out_height = proc_cfg.height;
}

}  // namespace processors
}  // namespace video2x
