#pragma once

#include <filesystem>
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "processor.h"

namespace video2x {
namespace processors {

// Real-ESRGAN running on Core ML (Apple GPU), used in place of the ncnn filter on macOS
class FilterCoreML : public Filter {
   public:
    FilterCoreML(int scaling_factor, std::filesystem::path model_path);
    ~FilterCoreML() override;

    int init(AVCodecContext* dec_ctx, AVCodecContext* enc_ctx, AVBufferRef* hw_ctx) override;
    int filter(AVFrame* in_frame, AVFrame** out_frame) override;
    ProcessorType get_processor_type() const override { return ProcessorType::RealESRGAN; }
    void get_output_dimensions(
        const ProcessorConfig& proc_cfg,
        int in_width,
        int in_height,
        int& out_width,
        int& out_height
    ) const override;

   private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    int scaling_factor_;
    std::filesystem::path model_path_;
    AVRational in_time_base_;
    AVRational out_time_base_;
    AVPixelFormat out_pix_fmt_;
};

}  // namespace processors
}  // namespace video2x
