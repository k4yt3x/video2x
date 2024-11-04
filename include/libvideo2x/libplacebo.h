#ifndef PLACEBO_H
#define PLACEBO_H

#include <filesystem>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
}

int init_libplacebo(
    AVFilterGraph **filter_graph,
    AVFilterContext **buffersrc_ctx,
    AVFilterContext **buffersink_ctx,
    AVCodecContext *dec_ctx,
    int out_width,
    int out_height,
    uint32_t vk_device_index,
    const std::filesystem::path &shader_path
);

#endif  // PLACEBO_H
