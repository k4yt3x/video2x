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

enum ProcessingMode {
    PROCESSING_MODE_FILTER,
    PROCESSING_MODE_INTERPOLATE,
};

enum ProcessorType {
    PROCESSOR_LIBPLACEBO,
    PROCESSOR_REALESRGAN,
    PROCESSOR_RIFE,
};

enum Libvideo2xLogLevel {
    LIBVIDEO2X_LOG_LEVEL_TRACE,
    LIBVIDEO2X_LOG_LEVEL_DEBUG,
    LIBVIDEO2X_LOG_LEVEL_INFO,
    LIBVIDEO2X_LOG_LEVEL_WARNING,
    LIBVIDEO2X_LOG_LEVEL_ERROR,
    LIBVIDEO2X_LOG_LEVEL_CRITICAL,
    LIBVIDEO2X_LOG_LEVEL_OFF
};

struct LibplaceboConfig {
    const CharType *shader_path;
};

struct RealESRGANConfig {
    bool tta_mode;
    const CharType *model_name;
};

struct RIFEConfig {
    bool tta_mode;
    bool tta_temporal_mode;
    bool uhd_mode;
    int num_threads;
    bool rife_v2;
    bool rife_v4;
    const CharType *model_name;
};

// Unified filter configuration
struct ProcessorConfig {
    enum ProcessorType processor_type;
    int width;
    int height;
    int scaling_factor;
    int frame_rate_multiplier;
    union {
        struct LibplaceboConfig libplacebo;
        struct RealESRGANConfig realesrgan;
        struct RIFEConfig rife;
    } config;
};

// Encoder configurations
struct EncoderConfig {
    // Non-AVCodecContext options
    enum AVCodecID codec;
    bool copy_streams;

    // Basic video options
    int width;
    int height;
    enum AVPixelFormat pix_fmt;

    // Rate control and compression
    int64_t bit_rate;
    int rc_buffer_size;
    int rc_min_rate;
    int rc_max_rate;
    int qmin;
    int qmax;

    // GOP and frame structure
    int gop_size;
    int max_b_frames;
    int keyint_min;
    int refs;

    // Performance and threading
    int thread_count;

    // Latency and buffering
    int delay;

    // Extra AVOptions
    struct {
        const char *key;
        const char *value;
    } *extra_options;
    size_t nb_extra_options;
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
    const struct ProcessorConfig *filter_config,
    struct EncoderConfig *encoder_config,
    struct VideoProcessingContext *proc_ctx
);

#ifdef __cplusplus
}
#endif

#endif  // LIBVIDEO2X_H
