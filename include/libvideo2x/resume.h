#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "encoder.h"
#include "libvideo2x_export.h"
#include "processor.h"

namespace video2x {
namespace resume {

inline constexpr char ARTIFACT_EXTENSION[] = ".v2xresume";
inline constexpr char MANIFEST_SIGNATURE[] = "VIDEO2X_RESUME";
inline constexpr uint32_t MANIFEST_VERSION = 1;

enum class Status {
    Active,
    Completed,
};

struct ResumeInfo {
    uint32_t version = 0;
    Status status = Status::Active;
    std::filesystem::path artifact_path;
    std::filesystem::path input_path;
    std::filesystem::path output_path;
    processors::ProcessorConfig processor_config;
    encoder::EncoderConfig encoder_config;
    uint32_t vk_device_index = 0;
    AVHWDeviceType hw_device_type = AV_HWDEVICE_TYPE_NONE;
    int video_stream_index = -1;
    int source_width = 0;
    int source_height = 0;
    int source_pixel_format = AV_PIX_FMT_NONE;
    AVRational source_time_base = {0, 1};
    AVRational source_frame_rate = {0, 1};
    int64_t total_source_frames = 0;
    bool total_source_frames_exact = false;
    int64_t completed_source_frames = 0;
    int64_t completed_output_frames = 0;
    int64_t checkpoint_timestamp = 0;
    std::string processor_name;
    std::string model_name;
};

LIBVIDEO2X_API bool is_resume_artifact(const std::filesystem::path& artifact);

LIBVIDEO2X_API int
inspect_resume_artifact(const std::filesystem::path& artifact, ResumeInfo& info, std::string& error);

LIBVIDEO2X_API int restart_resume_artifact(
    const std::filesystem::path& artifact,
    ResumeInfo& info,
    std::string& error
);

}  // namespace resume
}  // namespace video2x
