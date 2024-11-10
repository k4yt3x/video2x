#include "avutils.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
}

#include <spdlog/spdlog.h>

int64_t get_video_frame_count(AVFormatContext *ifmt_ctx, int in_vstream_idx) {
    // Use the 'nb_frames' field if it is available
    int64_t nb_frames = ifmt_ctx->streams[in_vstream_idx]->nb_frames;
    if (nb_frames != AV_NOPTS_VALUE && nb_frames > 0) {
        spdlog::debug("Read total number of frames from 'nb_frames': {}", nb_frames);
        return nb_frames;
    }
    spdlog::warn("Estimating the total number of frames from duration * fps");

    // Get the duration of the video
    double duration_secs = 0.0;
    if (ifmt_ctx->duration != AV_NOPTS_VALUE) {
        duration_secs = static_cast<double>(ifmt_ctx->duration) / static_cast<double>(AV_TIME_BASE);
    } else if (ifmt_ctx->streams[in_vstream_idx]->duration != AV_NOPTS_VALUE) {
        duration_secs = static_cast<double>(ifmt_ctx->streams[in_vstream_idx]->duration) *
                        av_q2d(ifmt_ctx->streams[in_vstream_idx]->time_base);
    }
    if (duration_secs <= 0) {
        spdlog::warn("Unable to determine the video's duration");
        return -1;
    }
    spdlog::debug("Video duration: {}s", duration_secs);

    // Calculate average FPS
    double fps = av_q2d(ifmt_ctx->streams[in_vstream_idx]->avg_frame_rate);
    if (fps <= 0) {
        spdlog::debug("Unable to read the average frame rate from 'avg_frame_rate'");
        fps = av_q2d(ifmt_ctx->streams[in_vstream_idx]->r_frame_rate);
    }
    if (fps <= 0) {
        spdlog::debug("Unable to read the average frame rate from 'r_frame_rate'");
        fps = av_q2d(av_guess_frame_rate(ifmt_ctx, ifmt_ctx->streams[in_vstream_idx], nullptr));
    }
    if (fps <= 0) {
        spdlog::debug("Unable to estimate the average frame rate with 'av_guess_frame_rate'");
        fps = av_q2d(ifmt_ctx->streams[in_vstream_idx]->time_base);
    }
    if (fps <= 0) {
        spdlog::warn("Unable to estimate the video's average frame rate");
        return -1;
    }
    spdlog::debug("Video average frame rate: {}", fps);

    // Estimate and return the total number of frames
    return static_cast<int64_t>(duration_secs * fps);
}

enum AVPixelFormat
get_encoder_default_pix_fmt(const AVCodec *encoder, AVPixelFormat target_pix_fmt) {
    int ret;
    char errbuf[AV_ERROR_MAX_STRING_SIZE];

    // Retrieve the list of supported pixel formats
#if LIBAVCODEC_BUILD >= CALC_FFMPEG_VERSION(61, 13, 100)
    const enum AVPixelFormat *supported_pix_fmts = nullptr;
    ret = avcodec_get_supported_config(
        nullptr, encoder, AV_CODEC_CONFIG_PIX_FORMAT, 0, (const void **)&supported_pix_fmts, nullptr
    );
    if (ret < 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        spdlog::error("Failed to get supported pixel formats: {}", errbuf);
        return AV_PIX_FMT_NONE;
    }

    if (supported_pix_fmts == nullptr) {
        if (target_pix_fmt == AV_PIX_FMT_NONE) {
            spdlog::warn("Encoder supports all pixel formats; defaulting to yuv420p");
            return AV_PIX_FMT_YUV420P;
        } else {
            spdlog::warn("Encoder supports all pixel formats; defaulting to the decoder's format");
            return target_pix_fmt;
        }
    }
#else
    const enum AVPixelFormat *supported_pix_fmts = encoder->pix_fmts;
#endif

    // Determine if the target pixel format has an alpha channel
    const AVPixFmtDescriptor *desc = nullptr;
    int has_alpha = 0;
    if (target_pix_fmt != AV_PIX_FMT_NONE) {
        desc = av_pix_fmt_desc_get(target_pix_fmt);
        has_alpha = desc ? (desc->nb_components % 2 == 0) : 0;
    }

    // Iterate over supported pixel formats to find the best match
    enum AVPixelFormat best_pix_fmt = AV_PIX_FMT_NONE;
    for (const enum AVPixelFormat *p = supported_pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
        if (target_pix_fmt != AV_PIX_FMT_NONE) {
            best_pix_fmt =
                av_find_best_pix_fmt_of_2(best_pix_fmt, *p, target_pix_fmt, has_alpha, nullptr);
            if (*p == target_pix_fmt) {
                best_pix_fmt = target_pix_fmt;
                break;
            }
        } else {
            best_pix_fmt = *p;
            break;
        }
    }
    if (best_pix_fmt == AV_PIX_FMT_NONE) {
        spdlog::error("No suitable pixel format found for encoder");
    }

    if (target_pix_fmt != AV_PIX_FMT_NONE && best_pix_fmt != target_pix_fmt) {
        spdlog::warn(
            "Incompatible pixel format '%s' for encoder '%s'; auto-selecting format '%s'",
            av_get_pix_fmt_name(target_pix_fmt),
            encoder->name,
            av_get_pix_fmt_name(best_pix_fmt)
        );
    }

    return best_pix_fmt;
}
