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
    virtual ~Filter() = default;
    virtual int init(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx, AVBufferRef *hw_ctx) = 0;
    virtual int process_frame(AVFrame *in_frame, AVFrame **out_frame) = 0;
    virtual int flush(std::vector<AVFrame *> &_) { return 0; }
};

#endif  // FILTER_H
