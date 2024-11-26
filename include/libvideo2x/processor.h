#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavutil/buffer.h>
}

#include "libvideo2x.h"

class Processor {
   public:
    virtual ~Processor() = default;
    virtual int init(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx, AVBufferRef *hw_ctx) = 0;
    virtual ProcessingMode get_processing_mode() const = 0;
    virtual int flush(std::vector<AVFrame *> &_) { return 0; }
};

// Abstract base class for filters
class Filter : public Processor {
   public:
    ProcessingMode get_processing_mode() const override { return PROCESSING_MODE_FILTER; }
    virtual int filter(AVFrame *in_frame, AVFrame **out_frame) = 0;
    virtual FilterType get_filter_type() const = 0;
};

// Abstract base class for interpolators
class Interpolator : public Processor {
   public:
    ProcessingMode get_processing_mode() const override { return PROCESSING_MODE_INTERPOLATE; }
    virtual int interpolate(AVFrame *in_frame, AVFrame *prev_frame, AVFrame **out_frame) = 0;
    virtual InterpolatorType get_interpolator_type() const = 0;
};

#endif  // PROCESSOR_H
