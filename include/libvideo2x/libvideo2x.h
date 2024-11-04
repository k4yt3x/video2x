#ifndef LIBVIDEO2X_H
#define LIBVIDEO2X_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#ifdef __cplusplus
}
#endif

#include "char_defs.h"

#ifdef _WIN32
#ifdef LIBVIDEO2X_EXPORTS
#define LIBVIDEO2X_API __declspec(dllexport)
#else
#define LIBVIDEO2X_API __declspec(dllimport)
#endif
#else
#define LIBVIDEO2X_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Enum to specify filter type
enum FilterType {
    FILTER_LIBPLACEBO,
    FILTER_REALESRGAN
};

// Enum to specify log level
enum Libvideo2xLogLevel {
    LIBVIDEO2X_LOG_LEVEL_TRACE,
    LIBVIDEO2X_LOG_LEVEL_DEBUG,
    LIBVIDEO2X_LOG_LEVEL_INFO,
    LIBVIDEO2X_LOG_LEVEL_WARNING,
    LIBVIDEO2X_LOG_LEVEL_ERROR,
    LIBVIDEO2X_LOG_LEVEL_CRITICAL,
    LIBVIDEO2X_LOG_LEVEL_OFF
};

// Configuration for Libplacebo filter
struct LibplaceboConfig {
    int out_width;
    int out_height;
    const CharType *shader_path;
};

// Configuration for RealESRGAN filter
struct RealESRGANConfig {
    bool tta_mode;
    int scaling_factor;
    const CharType *model_name;
};

// Unified filter configuration
struct FilterConfig {
    enum FilterType filter_type;
    union {
        struct LibplaceboConfig libplacebo;
        struct RealESRGANConfig realesrgan;
    } config;
};

// Encoder configuration
struct EncoderConfig {
    int out_width;
    int out_height;
    bool copy_streams;
    enum AVCodecID codec;
    enum AVPixelFormat pix_fmt;
    const char *preset;
    int64_t bit_rate;
    float crf;
};

// Video processing context
struct VideoProcessingContext {
    int64_t processed_frames;
    int64_t total_frames;
    time_t start_time;
    bool pause;
    bool abort;
    bool completed;
};

/**
 * @brief Process a video file using the selected filter and encoder settings.
 *
 * @param[in] in_fname Path to the input video file
 * @param[in] out_fname Path to the output video file
 * @param[in] log_level Log level
 * @param[in] benchmark Flag to enable benchmarking mode
 * @param[in] vk_device_index Vulkan device index
 * @param[in] hw_type Hardware device type
 * @param[in] filter_config Filter configurations
 * @param[in] encoder_config Encoder configurations
 * @param[in,out] proc_ctx Video processing context
 * @return int 0 on success, non-zero value on error
 */
LIBVIDEO2X_API int process_video(
    const CharType *in_fname,
    const CharType *out_fname,
    enum Libvideo2xLogLevel log_level,
    bool benchmark,
    uint32_t vk_device_index,
    enum AVHWDeviceType hw_device_type,
    const struct FilterConfig *filter_config,
    struct EncoderConfig *encoder_config,
    struct VideoProcessingContext *proc_ctx
);

#ifdef __cplusplus
}
#endif

#endif  // LIBVIDEO2X_H
