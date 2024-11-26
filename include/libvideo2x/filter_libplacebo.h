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
    AVFilterGraph *filter_graph;
    AVFilterContext *buffersrc_ctx;
    AVFilterContext *buffersink_ctx;
    uint32_t vk_device_index;
    const std::filesystem::path shader_path;
    int out_width;
    int out_height;
    AVRational in_time_base;
    AVRational out_time_base;

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
    FilterType get_filter_type() const override { return FILTER_LIBPLACEBO; }
};

#endif  // FILTER_LIBPLACEBO_H
