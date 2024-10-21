#include "libplacebo_filter.h"

#include <cstdio>

#include <spdlog/spdlog.h>

#include "fsutils.h"
#include "libplacebo.h"

LibplaceboFilter::LibplaceboFilter(
    int out_width,
    int out_height,
    const std::filesystem::path &shader_path
)
    : filter_graph(nullptr),
      buffersrc_ctx(nullptr),
      buffersink_ctx(nullptr),
      out_width(out_width),
      out_height(out_height),
      shader_path(std::move(shader_path)) {}

LibplaceboFilter::~LibplaceboFilter() {
    if (buffersrc_ctx) {
        avfilter_free(buffersrc_ctx);
        buffersrc_ctx = nullptr;
    }
    if (buffersink_ctx) {
        avfilter_free(buffersink_ctx);
        buffersink_ctx = nullptr;
    }
    if (filter_graph) {
        avfilter_graph_free(&filter_graph);
        filter_graph = nullptr;
    }
}

int LibplaceboFilter::init(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx, AVBufferRef *hw_ctx) {
    // Construct the shader path
    std::filesystem::path shader_full_path;
    if (filepath_is_readable(shader_path)) {
        // If the shader path is directly readable, use it
        shader_full_path = shader_path;
    } else {
        // Construct the fallback path using std::filesystem
        shader_full_path =
            find_resource_file(std::filesystem::path("models") / (shader_path.string() + ".glsl"));
    }

    // Check if the shader file exists
    if (!std::filesystem::exists(shader_full_path)) {
        spdlog::error("libplacebo shader file not found: {}", shader_full_path.string());
        return -1;
    }

    // Save the output time base
    in_time_base = dec_ctx->time_base;
    out_time_base = enc_ctx->time_base;

    return init_libplacebo(
        hw_ctx,
        &filter_graph,
        &buffersrc_ctx,
        &buffersink_ctx,
        dec_ctx,
        out_width,
        out_height,
        shader_full_path
    );
}

int LibplaceboFilter::process_frame(AVFrame *in_frame, AVFrame **out_frame) {
    int ret;

    // Get the filtered frame
    *out_frame = av_frame_alloc();
    if (*out_frame == nullptr) {
        spdlog::error("Failed to allocate output frame");
        return -1;
    }

    // Feed the frame to the filter graph
    ret = av_buffersrc_add_frame(buffersrc_ctx, in_frame);
    if (ret < 0) {
        spdlog::error("Error while feeding the filter graph");
        return ret;
    }

    ret = av_buffersink_get_frame(buffersink_ctx, *out_frame);
    if (ret < 0) {
        av_frame_free(out_frame);
        return ret;
    }

    // Rescale PTS to encoder's time base
    (*out_frame)->pts = av_rescale_q((*out_frame)->pts, in_time_base, out_time_base);

    // Return the processed frame to the caller
    return 0;
}

int LibplaceboFilter::flush(std::vector<AVFrame *> &flushed_frames) {
    int ret = av_buffersrc_add_frame(buffersrc_ctx, nullptr);
    if (ret < 0) {
        spdlog::error("Error while flushing filter graph");
        return ret;
    }

    // Retrieve all remaining frames from the filter graph
    while (1) {
        AVFrame *filt_frame = av_frame_alloc();
        if (filt_frame == nullptr) {
            return AVERROR(ENOMEM);
        }

        ret = av_buffersink_get_frame(buffersink_ctx, filt_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_frame_free(&filt_frame);
            break;
        }
        if (ret < 0) {
            av_frame_free(&filt_frame);
            return ret;
        }

        // Rescale PTS to encoder's time base
        filt_frame->pts = av_rescale_q(filt_frame->pts, in_time_base, out_time_base);

        // Add to processed frames
        flushed_frames.push_back(filt_frame);
    }

    return 0;
}
