#ifndef LIBVIDEO2X_H
#define LIBVIDEO2X_H

#include <libavutil/pixfmt.h>
#include <stdint.h>
#include <time.h>

#include <libavcodec/avcodec.h>
#include <libavcodec/codec_id.h>

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

// Configuration for Libplacebo filter
struct LibplaceboConfig {
    int output_width;
    int output_height;
    const char *shader_path;
};

// Configuration for RealESRGAN filter
struct RealESRGANConfig {
    int gpuid;
    int tta_mode;
    int scaling_factor;
    const char *model;
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
    int output_width;
    int output_height;
    enum AVCodecID codec;
    enum AVPixelFormat pix_fmt;
    const char *preset;
    int64_t bit_rate;
    float crf;
};

// Processing status
struct ProcessingStatus {
    int64_t processed_frames;
    int64_t total_frames;
    time_t start_time;
};

// C-compatible process_video function
LIBVIDEO2X_API int process_video(
    const char *input_filename,
    const char *output_filename,
    const struct FilterConfig *filter_config,
    struct EncoderConfig *encoder_config,
    struct ProcessingStatus *status
);

#ifdef __cplusplus
}
#endif

#endif  // LIBVIDEO2X_H
