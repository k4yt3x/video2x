#include "conversions.h"

#include <cstddef>
#include <cstdio>

#include <spdlog/spdlog.h>

#include "logger_manager.h"

namespace video2x {
namespace conversions {

// Convert AVFrame format
[[gnu::target_clones("arch=x86-64-v4", "arch=x86-64-v3", "default")]]
AVFrame* convert_avframe_pix_fmt(AVFrame* src_frame, AVPixelFormat pix_fmt) {
    AVFrame* dst_frame = av_frame_alloc();
    if (dst_frame == nullptr) {
        logger()->error("Failed to allocate destination AVFrame.");
        return nullptr;
    }

    dst_frame->format = pix_fmt;
    dst_frame->width = src_frame->width;
    dst_frame->height = src_frame->height;

    // Allocate memory for the converted frame
    if (av_frame_get_buffer(dst_frame, 32) < 0) {
        logger()->error("Failed to allocate memory for AVFrame.");
        av_frame_free(&dst_frame);
        return nullptr;
    }

    // Create a SwsContext for pixel format conversion
    SwsContext* sws_ctx = sws_getContext(
        src_frame->width,
        src_frame->height,
        static_cast<AVPixelFormat>(src_frame->format),
        dst_frame->width,
        dst_frame->height,
        pix_fmt,
        SWS_BILINEAR,
        nullptr,
        nullptr,
        nullptr
    );

    if (sws_ctx == nullptr) {
        logger()->error("Failed to initialize swscale context.");
        av_frame_free(&dst_frame);
        return nullptr;
    }

    // Perform the conversion
    sws_scale(
        sws_ctx,
        src_frame->data,
        src_frame->linesize,
        0,
        src_frame->height,
        dst_frame->data,
        dst_frame->linesize
    );

    // Clean up
    sws_freeContext(sws_ctx);

    return dst_frame;
}

// Convert AVFrame to ncnn::Mat by copying the data
[[gnu::target_clones("arch=x86-64-v4", "arch=x86-64-v3", "default")]]
ncnn::Mat avframe_to_ncnn_mat(AVFrame* frame) {
    AVFrame* converted_frame = nullptr;

    // Convert to BGR24 format if necessary
    if (frame->format != AV_PIX_FMT_BGR24) {
        converted_frame = convert_avframe_pix_fmt(frame, AV_PIX_FMT_BGR24);
        if (!converted_frame) {
            logger()->error("Failed to convert AVFrame to BGR24.");
            return ncnn::Mat();
        }
    } else {
        // If the frame is already in BGR24, use it directly
        converted_frame = frame;
    }

    // Allocate a new ncnn::Mat and copy the data
    int width = converted_frame->width;
    int height = converted_frame->height;
    ncnn::Mat ncnn_image = ncnn::Mat(width, height, static_cast<size_t>(3), 3);

    // Manually copy the pixel data from AVFrame to the new ncnn::Mat
    const uint8_t* src_data = converted_frame->data[0];
    for (int y = 0; y < height; y++) {
        uint8_t* dst_row = ncnn_image.row<uint8_t>(y);
        const uint8_t* src_row = src_data + y * converted_frame->linesize[0];

        // Copy 3 channels (BGR) per pixel
        memcpy(dst_row, src_row, static_cast<size_t>(width) * 3);
    }

    // If we allocated a converted frame, free it
    if (converted_frame != frame) {
        av_frame_free(&converted_frame);
    }

    return ncnn_image;
}

// Convert ncnn::Mat to AVFrame with a specified pixel format (this part is unchanged)
[[gnu::target_clones("arch=x86-64-v4", "arch=x86-64-v3", "default")]]
AVFrame* ncnn_mat_to_avframe(const ncnn::Mat& mat, AVPixelFormat pix_fmt) {
    int ret;

    // Step 1: Allocate a destination AVFrame for the specified pixel format
    AVFrame* dst_frame = av_frame_alloc();
    if (!dst_frame) {
        logger()->error("Failed to allocate destination AVFrame.");
        return nullptr;
    }

    dst_frame->format = pix_fmt;
    dst_frame->width = mat.w;
    dst_frame->height = mat.h;

    // Allocate memory for the frame buffer
    if (av_frame_get_buffer(dst_frame, 32) < 0) {
        logger()->error("Failed to allocate memory for destination AVFrame.");
        av_frame_free(&dst_frame);
        return nullptr;
    }

    // Step 2: Convert ncnn::Mat to BGR AVFrame
    AVFrame* bgr_frame = av_frame_alloc();
    if (!bgr_frame) {
        logger()->error("Failed to allocate intermediate BGR AVFrame.");
        av_frame_free(&dst_frame);
        return nullptr;
    }

    bgr_frame->format = AV_PIX_FMT_BGR24;
    bgr_frame->width = mat.w;
    bgr_frame->height = mat.h;

    // Allocate memory for the intermediate BGR frame
    if (av_frame_get_buffer(bgr_frame, 32) < 0) {
        logger()->error("Failed to allocate memory for BGR AVFrame.");
        av_frame_free(&dst_frame);
        av_frame_free(&bgr_frame);
        return nullptr;
    }

    // Copy the pixel data from ncnn::Mat to the BGR AVFrame
    for (int y = 0; y < mat.h; y++) {
        uint8_t* dst_row = bgr_frame->data[0] + y * bgr_frame->linesize[0];
        const uint8_t* src_row = mat.row<const uint8_t>(y);

        // Copy 3 channels (BGR) per pixel
        memcpy(dst_row, src_row, static_cast<size_t>(mat.w) * 3);
    }

    // Step 3: Convert the BGR frame to the desired pixel format
    SwsContext* sws_ctx = sws_getContext(
        bgr_frame->width,
        bgr_frame->height,
        AV_PIX_FMT_BGR24,
        dst_frame->width,
        dst_frame->height,
        pix_fmt,
        SWS_BILINEAR,
        nullptr,
        nullptr,
        nullptr
    );

    if (sws_ctx == nullptr) {
        logger()->error("Failed to initialize swscale context.");
        av_frame_free(&bgr_frame);
        av_frame_free(&dst_frame);
        return nullptr;
    }

    // Perform the conversion
    ret = sws_scale(
        sws_ctx,
        bgr_frame->data,
        bgr_frame->linesize,
        0,
        bgr_frame->height,
        dst_frame->data,
        dst_frame->linesize
    );

    // Clean up
    sws_freeContext(sws_ctx);
    av_frame_free(&bgr_frame);

    if (ret != dst_frame->height) {
        logger()->error("Failed to convert BGR AVFrame to destination pixel format.");
        av_frame_free(&dst_frame);
        return nullptr;
    }

    return dst_frame;
}

}  // namespace conversions
}  // namespace video2x
