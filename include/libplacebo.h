#ifndef PLACEBO_H
#define PLACEBO_H

#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>

int init_libplacebo(
    AVFilterGraph **filter_graph,
    AVFilterContext **buffersrc_ctx,
    AVFilterContext **buffersink_ctx,
    AVCodecContext *dec_ctx,
    int output_width,
    int output_height,
    const char *shader_path
);

#endif  // PLACEBO_H
