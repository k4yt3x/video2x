#include "avutils.h"

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
