#ifndef PLACEBO_H
#define PLACEBO_H

#include <filesystem>

#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavutil/buffer.h>

int init_libplacebo(
    AVFilterGraph **filter_graph,
    AVFilterContext **buffersrc_ctx,
    AVFilterContext **buffersink_ctx,
    AVBufferRef **device_ctx,
    AVCodecContext *dec_ctx,
    int output_width,
    int output_height,
    const std::filesystem::path &shader_path
);

#endif  // PLACEBO_H
