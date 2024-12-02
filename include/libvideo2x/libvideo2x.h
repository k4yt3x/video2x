#ifndef LIBVIDEO2X_H
#define LIBVIDEO2X_H

#include <filesystem>
#include <variant>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "fsutils.h"
#include "logging.h"

#ifdef _WIN32
#ifdef LIBVIDEO2X_EXPORTS
#define LIBVIDEO2X_API __declspec(dllexport)
#else
#define LIBVIDEO2X_API __declspec(dllimport)
#endif
#else
#define LIBVIDEO2X_API
#endif

enum class ProcessingMode {
    Filter,
    Interpolate,
};

enum class ProcessorType {
    Libplacebo,
    RealESRGAN,
    RIFE,
};

struct LibplaceboConfig {
    StringType shader_path;
};

struct RealESRGANConfig {
    bool tta_mode;
    StringType model_name;
};

struct RIFEConfig {
    bool tta_mode;
    bool tta_temporal_mode;
    bool uhd_mode;
    int num_threads;
    StringType model_name;
};

// Unified filter configuration
struct ProcessorConfig {
    enum ProcessorType processor_type;
    int width;
    int height;
    int scaling_factor;
    int frm_rate_mul;
    float scn_det_thresh;
    std::variant<LibplaceboConfig, RealESRGANConfig, RIFEConfig> config;
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
    std::vector<std::pair<StringType, StringType>> extra_opts;
};

struct HardwareConfig {
    uint32_t vk_device_index;
    AVHWDeviceType hw_device_type;
};

// Video processing context
struct VideoProcessingContext {
    int64_t processed_frames;
    int64_t total_frames;
    std::time_t start_time;
    bool pause;
    bool abort;
    bool completed;
};

// Process a video file using the specified configurations
[[nodiscard]] LIBVIDEO2X_API int process_video(
    const std::filesystem::path in_fname,
    const std::filesystem::path out_fname,
    const HardwareConfig hw_cfg,
    const ProcessorConfig proc_cfg,
    EncoderConfig enc_cfg,
    VideoProcessingContext *proc_ctx,
    Libvideo2xLogLevel log_level,
    bool benchmark
);

#endif  // LIBVIDEO2X_H
