#ifndef FILTER_H
#define FILTER_H

#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
}

// Abstract base class for filters
class Filter {
   public:
    virtual ~Filter() {}
    virtual int init(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx) = 0;
    virtual AVFrame *process_frame(AVFrame *input_frame) = 0;
    virtual int flush(std::vector<AVFrame *> &processed_frames) = 0;
};

#endif  // FILTER_H
