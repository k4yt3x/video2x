#include "conversions.h"

#include <cstddef>
#include <cstdio>
#include <mutex>

extern "C" {
#include <libavutil/pixdesc.h>
}

#include <spdlog/spdlog.h>

#include "logger_manager.h"

namespace video2x {
namespace conversions {

// Resampling filter used for all pixel format conversions. SWS_BICUBIC is
// sharper than bilinear while avoiding the ringing/shimmer of Lanczos/Sinc.
// Both conversion directions use the same filter so
// the round-trip is symmetric and does not introduce asymmetric chroma error.
static constexpr int SWS_FILTER = SWS_BICUBIC;

// Map an AVColorSpace enum to the SWS_CS_* constant expected by sws_setColorspaceDetails.
// sws_getContext always defaults to BT.601; call sws_setColorspaceDetails after to override.
static int avcol_spc_to_sws_cs(AVColorSpace cs) {
    switch (cs) {
        case AVCOL_SPC_BT709:
            return SWS_CS_ITU709;
        case AVCOL_SPC_FCC:
            return SWS_CS_FCC;
        case AVCOL_SPC_BT470BG:
            return SWS_CS_ITU601;
        case AVCOL_SPC_SMPTE170M:
            return SWS_CS_SMPTE170M;
        case AVCOL_SPC_SMPTE240M:
            return SWS_CS_SMPTE240M;
        case AVCOL_SPC_BT2020_NCL:
            return SWS_CS_BT2020;
        case AVCOL_SPC_BT2020_CL:
            return SWS_CS_BT2020;
        default:
            if (cs == AVCOL_SPC_UNSPECIFIED) {
                static std::once_flag warned;
                std::call_once(warned, [] {
                    logger()->warn(
                        "Color space is AVCOL_SPC_UNSPECIFIED; "
                        "falling back to BT.601. Output colors may be incorrect "
                        "for BT.709 or BT.2020 content."
                    );
                });
            }
            // Default to BT.601 using SWS_CS_DEFAULT
            // Assuming BT.709 could silently mis-color
            return SWS_CS_DEFAULT;
    }
}

// Override the default BT.601 colorspace matrix on an already-created SwsContext.
// src_range/dst_range: 0 = limited (TV), 1 = full (JPEG/PC).
// Brightness/contrast/saturation are left at identity (0, 1.0, 1.0).
static void
apply_sws_colorspace(SwsContext* sws_ctx, const int* coeffs, int src_range, int dst_range) {
    int ret = sws_setColorspaceDetails(
        sws_ctx,
        coeffs,
        src_range,
        coeffs,
        dst_range,
        0,        // brightness: 0.0 in 16.16 fixed-point (additive offset, no adjustment)
        1 << 16,  // contrast:   1.0 in 16.16 fixed-point (multiplicative offset, no adjustment)
        1 << 16   // saturation: 1.0 in 16.16 fixed-point (multiplicative offset, no adjustment)
    );
    if (ret < 0) {
        static std::atomic<int> fail_count{0};
        int count = ++fail_count;
        if (count == 1) {
            logger()->warn(
                "sws_setColorspaceDetails failed; colorspace conversion may be inaccurate."
            );
        } else {
            logger()->debug(
                "sws_setColorspaceDetails failed (occurrence #{}); "
                "colorspace conversion may be inaccurate.",
                count
            );
        }
    }
}

// Convert AVFrame format
[[gnu::target_clones("arch=x86-64-v4", "arch=x86-64-v3", "default")]]
AVFrame* convert_avframe_pix_fmt(AVFrame* src_frame, AVPixelFormat pix_fmt) {
    int ret;

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
        SWS_FILTER,
        nullptr,
        nullptr,
        nullptr
    );

    if (sws_ctx == nullptr) {
        logger()->error("Failed to initialize swscale context.");
        av_frame_free(&dst_frame);
        return nullptr;
    }

    {
        int sws_cs = avcol_spc_to_sws_cs(src_frame->colorspace);
        int src_range = (src_frame->color_range == AVCOL_RANGE_JPEG) ? 1 : 0;
        const AVPixFmtDescriptor* dst_desc = av_pix_fmt_desc_get(pix_fmt);
        int dst_range = (dst_desc && (dst_desc->flags & AV_PIX_FMT_FLAG_RGB)) ? 1 : src_range;
        // Override sws's default BT.601 matrix with the source frame's actual colorspace.
        // RGB output is always full-range; YUV output keeps the source range.
        apply_sws_colorspace(sws_ctx, sws_getCoefficients(sws_cs), src_range, dst_range);
    }

    // Perform the conversion
    ret = sws_scale(
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

    if (ret != dst_frame->height) {
        logger()->error("Failed to convert AVFrame pixel format.");
        av_frame_free(&dst_frame);
        return nullptr;
    }

    return dst_frame;
}

// Convert AVFrame to ncnn::Mat by copying the data
[[gnu::target_clones("arch=x86-64-v4", "arch=x86-64-v3", "default")]]
ncnn::Mat avframe_to_ncnn_mat(AVFrame* frame, const AVFrame* color_hint) {
    // Fill in missing color metadata from the hint so convert_avframe_pix_fmt
    // uses the correct color matrix (decoders often leave per-frame colorspace
    // as AVCOL_SPC_UNSPECIFIED even when the stream header specifies it)
    if (frame->colorspace == AVCOL_SPC_UNSPECIFIED) {
        static std::once_flag warned;
        std::call_once(warned, [] {
            logger()->warn(
                "Decoded frame has AVCOL_SPC_UNSPECIFIED colorspace; "
                "color matrix will be taken from stream header via color_hint."
            );
        });
    }

    if (color_hint != nullptr) {
        if (frame->colorspace == AVCOL_SPC_UNSPECIFIED) {
            frame->colorspace = color_hint->colorspace;
        }
        if (frame->color_range == AVCOL_RANGE_UNSPECIFIED) {
            frame->color_range = color_hint->color_range;
        }
    }

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

// Convert ncnn::Mat to AVFrame with a specified pixel format
[[gnu::target_clones("arch=x86-64-v4", "arch=x86-64-v3", "default")]]
AVFrame*
ncnn_mat_to_avframe(const ncnn::Mat& mat, AVPixelFormat pix_fmt, const AVFrame* color_hint) {
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
        SWS_FILTER,
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

    // Override sws's default BT.601 matrix using the reference frame's colorspace.
    // ncnn::Mat pixel data is always full-range (0-255), so src_range=1.
    // The output range and color metadata are taken from the color_hint reference frame.
    if (color_hint != nullptr) {
        int sws_cs = avcol_spc_to_sws_cs(color_hint->colorspace);
        int dst_range = (color_hint->color_range == AVCOL_RANGE_JPEG) ? 1 : 0;
        // ncnn::Mat pixel data is always full-range (0-255), so src_range=1.
        // The output range and color metadata are taken from the color_hint reference frame.
        apply_sws_colorspace(sws_ctx, sws_getCoefficients(sws_cs), 1, dst_range);

        // ncnn::Mat carries no color metadata, so the output frame would have
        // all AVCOL_*_UNSPECIFIED fields without this. Copy the source video's
        // properties so the player can correctly interpret the output frame.
        dst_frame->colorspace = color_hint->colorspace;
        dst_frame->color_range = color_hint->color_range;
        dst_frame->color_primaries = color_hint->color_primaries;
        dst_frame->color_trc = color_hint->color_trc;
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
