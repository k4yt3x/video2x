#ifndef FILTER_LIBPLACEBO_H
#define FILTER_LIBPLACEBO_H

#include <filesystem>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}

#include "processor.h"

// FilterLibplacebo class definition
class FilterLibplacebo : public Filter {
   private:
    AVFilterGraph *filter_graph_;
    AVFilterContext *buffersrc_ctx_;
    AVFilterContext *buffersink_ctx_;
    uint32_t vk_device_index_;
    const std::filesystem::path shader_path_;
    int width_;
    int height_;
    AVRational in_time_base_;
    AVRational out_time_base_;

   public:
    // Constructor
    FilterLibplacebo(
        uint32_t vk_device_index,
        const std::filesystem::path &shader_path,
        int width,
        int height
    );

    // Destructor
    virtual ~FilterLibplacebo() override;

    // Initializes the filter with decoder and encoder contexts
    int init(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx, AVBufferRef *hw_ctx) override;

    // Processes an input frame and returns the processed frame
    int filter(AVFrame *in_frame, AVFrame **out_frame) override;

    // Flushes any remaining frames
    int flush(std::vector<AVFrame *> &flushed_frames) override;

    // Returns the filter's type
    ProcessorType get_processor_type() const override { return PROCESSOR_LIBPLACEBO; }
};

#endif  // FILTER_LIBPLACEBO_H
