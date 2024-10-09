#include "libplacebo_filter.h"

#include <cstdio>

#include "fsutils.h"
#include "libplacebo.h"

LibplaceboFilter::LibplaceboFilter(int width, int height, const std::filesystem::path &shader_path)
    : filter_graph(nullptr),
      buffersrc_ctx(nullptr),
      buffersink_ctx(nullptr),
      output_width(width),
      output_height(height),
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
        fprintf(stderr, "libplacebo shader file not found: %s\n", shader_full_path.c_str());
        return -1;
    }

    // Save the output time base
    output_time_base = enc_ctx->time_base;

    return init_libplacebo(
        hw_ctx,
        &filter_graph,
        &buffersrc_ctx,
        &buffersink_ctx,
        dec_ctx,
        output_width,
        output_height,
        shader_full_path
    );
}

int LibplaceboFilter::process_frame(AVFrame *input_frame, AVFrame **output_frame) {
    int ret;

    // Get the filtered frame
    *output_frame = av_frame_alloc();
    if (*output_frame == nullptr) {
        fprintf(stderr, "Failed to allocate output frame\n");
        return -1;
    }

    // Feed the frame to the filter graph
    ret = av_buffersrc_add_frame(buffersrc_ctx, input_frame);
    if (ret < 0) {
        fprintf(stderr, "Error while feeding the filter graph\n");
        return ret;
    }

    ret = av_buffersink_get_frame(buffersink_ctx, *output_frame);
    if (ret < 0) {
        av_frame_free(output_frame);
        return ret;
    }

    // Rescale PTS to encoder's time base
    (*output_frame)->pts =
        av_rescale_q((*output_frame)->pts, buffersink_ctx->inputs[0]->time_base, output_time_base);

    // Return the processed frame to the caller
    return 0;
}

int LibplaceboFilter::flush(std::vector<AVFrame *> &processed_frames) {
    int ret = av_buffersrc_add_frame(buffersrc_ctx, nullptr);
    if (ret < 0) {
        fprintf(stderr, "Error while flushing filter graph\n");
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
        filt_frame->pts =
            av_rescale_q(filt_frame->pts, buffersink_ctx->inputs[0]->time_base, output_time_base);

        // Add to processed frames
        processed_frames.push_back(filt_frame);
    }

    return 0;
}
