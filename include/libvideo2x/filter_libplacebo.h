#pragma once

#include <filesystem>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}

#include "processor.h"

namespace video2x {
namespace processors {

// FilterLibplacebo class definition
class FilterLibplacebo : public Filter {
   public:
    // Constructor
    FilterLibplacebo(
        uint32_t vk_device_index,
        const std::filesystem::path& shader_path,
        int width,
        int height
    );

    // Destructor
    virtual ~FilterLibplacebo() override;

    // Initializes the filter with decoder and encoder contexts
    int init(AVCodecContext* dec_ctx, AVCodecContext* enc_ctx, AVBufferRef* hw_ctx) override;

    // Processes an input frame and returns the processed frame
    int filter(AVFrame* in_frame, AVFrame** out_frame) override;

    // Flushes any remaining frames
    int flush(std::vector<AVFrame*>& flushed_frames) override;

    // Returns the filter's type
    ProcessorType get_processor_type() const override { return ProcessorType::Libplacebo; }

    // Returns the filter's output dimensions
    void get_output_dimensions(
        const ProcessorConfig& proc_cfg,
        int in_width,
        int in_height,
        int& out_width,
        int& out_height
    ) const override;

   private:
    AVFilterGraph* filter_graph_;
    AVFilterContext* buffersrc_ctx_;
    AVFilterContext* buffersink_ctx_;
    uint32_t vk_device_index_;
    const std::filesystem::path shader_path_;
    int width_;
    int height_;
    AVRational in_time_base_;
    AVRational out_time_base_;
};

}  // namespace processors
}  // namespace video2x
