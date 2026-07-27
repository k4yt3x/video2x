#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "libvideo2x/resume.h"

namespace video2x {
namespace resume {

class Session {
   public:
    Session(
        std::filesystem::path artifact,
        std::filesystem::path input,
        std::filesystem::path output,
        const processors::ProcessorConfig& processor_config,
        const encoder::EncoderConfig& encoder_config,
        uint32_t vk_device_index,
        AVHWDeviceType hw_device_type
    );

    int initialize(AVFormatContext* format, AVCodecContext* decoder, int video_stream, std::string& error);
    int load_unit(int64_t source_index, std::vector<AVFrame*>& frames, std::string& error) const;
    int store_unit(int64_t source_index, const std::vector<AVFrame*>& frames, std::string& error);
    int load_tail(std::vector<AVFrame*>& frames, std::string& error) const;
    int store_tail(const std::vector<AVFrame*>& frames, std::string& error);
    int checkpoint(int64_t source_frames, int64_t output_frames, bool force, std::string& error);
    void set_total_source_frames(int64_t source_frames);
    int complete(std::string& error);
    int publish_output(const std::filesystem::path& temporary_output, std::string& error);

    bool resumed() const { return resumed_; }
    int64_t completed_source_frames() const { return info_.completed_source_frames; }
    int64_t completed_output_frames() const { return info_.completed_output_frames; }
    const std::filesystem::path& artifact_path() const { return info_.artifact_path; }
    std::filesystem::path temporary_output_path() const;

   private:
    ResumeInfo info_;
    uintmax_t input_size_ = 0;
    int64_t input_mtime_ = 0;
    uint64_t input_fingerprint_ = 0;
    int64_t tail_frame_count_ = 0;
    bool resumed_ = false;
};

}  // namespace resume
}  // namespace video2x
