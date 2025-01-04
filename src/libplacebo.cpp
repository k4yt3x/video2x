#include "libplacebo.h"

#include <stdlib.h>
#include <string>

extern "C" {
#include <libavutil/dict.h>
#include <libavutil/opt.h>
}

#include <spdlog/spdlog.h>

#include "logger_manager.h"

namespace video2x {
namespace processors {

int init_libplacebo(
    AVFilterGraph** filter_graph,
    AVFilterContext** buffersrc_ctx,
    AVFilterContext** buffersink_ctx,
    AVCodecContext* dec_ctx,
    int out_width,
    int out_height,
    uint32_t vk_device_index,
    const std::filesystem::path& shader_path
) {
    int ret;

    // Create the Vulkan hardware device context
    AVBufferRef* vk_hw_device_ctx = nullptr;
    ret = av_hwdevice_ctx_create(
        &vk_hw_device_ctx,
        AV_HWDEVICE_TYPE_VULKAN,
        std::to_string(vk_device_index).c_str(),
        nullptr,
        0
    );
    if (ret < 0) {
        logger()->error("Failed to create Vulkan hardware device context for libplacebo.");
        vk_hw_device_ctx = nullptr;
    }

    AVFilterGraph* graph = avfilter_graph_alloc();
    if (!graph) {
        logger()->error("Unable to create filter graph.");
        return AVERROR(ENOMEM);
    }

    // Create buffer source
    const AVFilter* buffersrc = avfilter_get_by_name("buffer");
    if (!buffersrc) {
        logger()->error("Filter 'buffer' not found.");
        avfilter_graph_free(&graph);
        return AVERROR_FILTER_NOT_FOUND;
    }

    // Start building the arguments string
    std::string args = "video_size=" + std::to_string(dec_ctx->width) + "x" +
                       std::to_string(dec_ctx->height) +
                       ":pix_fmt=" + std::to_string(dec_ctx->pix_fmt) +
                       ":time_base=" + std::to_string(dec_ctx->time_base.num) + "/" +
                       std::to_string(dec_ctx->time_base.den) +
                       ":frame_rate=" + std::to_string(dec_ctx->framerate.num) + "/" +
                       std::to_string(dec_ctx->framerate.den) +
                       ":pixel_aspect=" + std::to_string(dec_ctx->sample_aspect_ratio.num) + "/" +
                       std::to_string(dec_ctx->sample_aspect_ratio.den);

    // Make a copy of the AVClass on the stack
    AVClass priv_class_copy = *buffersrc->priv_class;
    AVClass* priv_class_copy_ptr = &priv_class_copy;

    // Check if the colorspace option is supported
    if (av_opt_find(&priv_class_copy_ptr, "colorspace", nullptr, 0, AV_OPT_SEARCH_FAKE_OBJ)) {
        args += ":colorspace=" + std::to_string(dec_ctx->colorspace);
    } else {
        logger()->warn("Option 'colorspace' is not supported by the buffer filter.");
    }

    // Check if the range option is supported
    if (av_opt_find(&priv_class_copy_ptr, "range", nullptr, 0, AV_OPT_SEARCH_FAKE_OBJ)) {
        args += ":range=" + std::to_string(dec_ctx->color_range);
    } else {
        logger()->warn("Option 'range' is not supported by the buffer filter.");
    }

    logger()->debug("Buffer source args: {}", args);
    ret =
        avfilter_graph_create_filter(buffersrc_ctx, buffersrc, "in", args.c_str(), nullptr, graph);
    if (ret < 0) {
        logger()->error("Cannot create buffer source.");
        avfilter_graph_free(&graph);
        return ret;
    }

    AVFilterContext* last_filter = *buffersrc_ctx;

    // Create the libplacebo filter
    const AVFilter* libplacebo_filter = avfilter_get_by_name("libplacebo");
    if (!libplacebo_filter) {
        logger()->error("Filter 'libplacebo' not found.");
        avfilter_graph_free(&graph);
        return AVERROR_FILTER_NOT_FOUND;
    }

    // Convert the shader path to a string since filter args is const char *
    std::string shader_path_string = shader_path.u8string();

#ifdef _WIN32
    // libplacebo does not recognize the Windows '\\' path separator
    std::replace(shader_path_string.begin(), shader_path_string.end(), '\\', '/');
#endif

    // Prepare the filter arguments
    std::string filter_args = "w=" + std::to_string(out_width) +
                              ":h=" + std::to_string(out_height) + ":custom_shader_path='" +
                              shader_path_string + "'";

    AVFilterContext* libplacebo_ctx;
    ret = avfilter_graph_create_filter(
        &libplacebo_ctx, libplacebo_filter, "libplacebo", filter_args.c_str(), nullptr, graph
    );
    if (ret < 0) {
        logger()->error("Cannot create libplacebo filter.");
        avfilter_graph_free(&graph);
        return ret;
    }

    // Set the hardware device context to Vulkan
    if (vk_hw_device_ctx != nullptr) {
        libplacebo_ctx->hw_device_ctx = av_buffer_ref(vk_hw_device_ctx);
        av_buffer_unref(&vk_hw_device_ctx);
    }

    // Link buffersrc to libplacebo
    ret = avfilter_link(last_filter, 0, libplacebo_ctx, 0);
    if (ret < 0) {
        logger()->error("Error connecting buffersrc to libplacebo filter.");
        avfilter_graph_free(&graph);
        return ret;
    }

    last_filter = libplacebo_ctx;

    // Create buffer sink
    const AVFilter* buffersink = avfilter_get_by_name("buffersink");
    ret = avfilter_graph_create_filter(buffersink_ctx, buffersink, "out", nullptr, nullptr, graph);
    if (ret < 0) {
        logger()->error("Cannot create buffer sink.");
        avfilter_graph_free(&graph);
        return ret;
    }

    // Link libplacebo to buffersink
    ret = avfilter_link(last_filter, 0, *buffersink_ctx, 0);
    if (ret < 0) {
        logger()->error("Error connecting libplacebo filter to buffersink.");
        avfilter_graph_free(&graph);
        return ret;
    }

    // Configure the filter graph
    ret = avfilter_graph_config(graph, nullptr);
    if (ret < 0) {
        logger()->error("Error configuring the filter graph.");
        avfilter_graph_free(&graph);
        return ret;
    }

    *filter_graph = graph;
    return 0;
}

}  // namespace processors
}  // namespace video2x
