#ifndef LIBPLACEBO_FILTER_H
#define LIBPLACEBO_FILTER_H

#include <filesystem>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}

#include "filter.h"

// LibplaceboFilter class definition
class LibplaceboFilter : public Filter {
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
    LibplaceboFilter(
        uint32_t vk_device_index,
        const std::filesystem::path &shader_path,
        int width,
        int height
    );

    // Destructor
    virtual ~LibplaceboFilter() override;

    // Initializes the filter with decoder and encoder contexts
    int init(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx, AVBufferRef *hw_ctx) override;

    // Processes an input frame and returns the processed frame
    int process_frame(AVFrame *in_frame, AVFrame **out_frame) override;

    // Flushes any remaining frames
    int flush(std::vector<AVFrame *> &flushed_frames) override;
};

#endif  // LIBPLACEBO_FILTER_H
