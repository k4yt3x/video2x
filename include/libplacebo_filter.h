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
    int output_width;
    int output_height;
    const std::filesystem::path shader_path;
    AVRational output_time_base;

   public:
    // Constructor
    LibplaceboFilter(int width, int height, const std::filesystem::path &shader_path);

    // Destructor
    virtual ~LibplaceboFilter();

    // Initializes the filter with decoder and encoder contexts
    int init(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx, AVBufferRef *hw_ctx) override;

    // Processes an input frame and returns the processed frame
    int process_frame(AVFrame *input_frame, AVFrame **output_frame) override;

    // Flushes any remaining frames
    int flush(std::vector<AVFrame *> &processed_frames) override;
};

#endif  // LIBPLACEBO_FILTER_H
