#ifndef FILTER_H
#define FILTER_H

#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavutil/buffer.h>
}

// Abstract base class for filters
class Filter {
   public:
    virtual ~Filter() {}
    virtual int init(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx, AVBufferRef *hw_ctx) = 0;
    virtual int process_frame(AVFrame *input_frame, AVFrame **output_frame) = 0;
    virtual int flush(std::vector<AVFrame *> &processed_frames) = 0;
};

#endif  // FILTER_H
