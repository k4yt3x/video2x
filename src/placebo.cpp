#include <stdio.h>
#include <stdlib.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/rational.h>
#include <libswscale/swscale.h>
}

int init_libplacebo(
    AVFilterGraph **filter_graph,
    AVFilterContext **buffersrc_ctx,
    AVFilterContext **buffersink_ctx,
    AVCodecContext *dec_ctx,
    int output_width,
    int output_height,
    const char *shader_path
) {
    char args[512];
    int ret;

    AVFilterGraph *graph = avfilter_graph_alloc();
    if (!graph) {
        fprintf(stderr, "Unable to create filter graph.\n");
        return AVERROR(ENOMEM);
    }

    // Create buffer source
    const AVFilter *buffersrc = avfilter_get_by_name("buffer");
    snprintf(
        args,
        sizeof(args),
        "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:frame_rate=%d/%d:"
        "pixel_aspect=%d/%d:colorspace=%d",
        dec_ctx->width,
        dec_ctx->height,
        dec_ctx->pix_fmt,
        dec_ctx->time_base.num,
        dec_ctx->time_base.den,
        dec_ctx->framerate.num,
        dec_ctx->framerate.den,
        dec_ctx->sample_aspect_ratio.num,
        dec_ctx->sample_aspect_ratio.den,
        dec_ctx->colorspace
    );

    ret = avfilter_graph_create_filter(buffersrc_ctx, buffersrc, "in", args, NULL, graph);
    if (ret < 0) {
        fprintf(stderr, "Cannot create buffer source\n");
        avfilter_graph_free(&graph);
        return ret;
    }

    AVFilterContext *last_filter = *buffersrc_ctx;

    // Create the libplacebo filter
    const AVFilter *libplacebo_filter = avfilter_get_by_name("libplacebo");
    if (!libplacebo_filter) {
        fprintf(stderr, "Filter 'libplacebo' not found\n");
        avfilter_graph_free(&graph);
        return AVERROR_FILTER_NOT_FOUND;
    }

    // Prepare the filter arguments
    char filter_args[512];
    snprintf(
        filter_args,
        sizeof(filter_args),
        "w=%d:h=%d:upscaler=ewa_lanczos:custom_shader_path=%s",
        output_width,
        output_height,
        shader_path
    );

    AVFilterContext *libplacebo_ctx;
    ret = avfilter_graph_create_filter(
        &libplacebo_ctx, libplacebo_filter, "libplacebo", filter_args, NULL, graph
    );
    if (ret < 0) {
        fprintf(stderr, "Cannot create libplacebo filter\n");
        avfilter_graph_free(&graph);
        return ret;
    }

    // Link buffersrc to libplacebo
    ret = avfilter_link(last_filter, 0, libplacebo_ctx, 0);
    if (ret < 0) {
        fprintf(stderr, "Error connecting buffersrc to libplacebo filter\n");
        avfilter_graph_free(&graph);
        return ret;
    }

    last_filter = libplacebo_ctx;

    // Create buffer sink
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    ret = avfilter_graph_create_filter(buffersink_ctx, buffersink, "out", NULL, NULL, graph);
    if (ret < 0) {
        fprintf(stderr, "Cannot create buffer sink\n");
        avfilter_graph_free(&graph);
        return ret;
    }

    // Link libplacebo to buffersink
    ret = avfilter_link(last_filter, 0, *buffersink_ctx, 0);
    if (ret < 0) {
        fprintf(stderr, "Error connecting libplacebo filter to buffersink\n");
        avfilter_graph_free(&graph);
        return ret;
    }

    // Configure the filter graph
    ret = avfilter_graph_config(graph, NULL);
    if (ret < 0) {
        fprintf(stderr, "Error configuring the filter graph\n");
        avfilter_graph_free(&graph);
        return ret;
    }

    *filter_graph = graph;
    return 0;
}
