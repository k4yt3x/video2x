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
    virtual int flush(std::vector<AVFrame *> &_) { return 0; }
    virtual ProcessingMode get_processing_mode() const = 0;
    virtual ProcessorType get_processor_type() const = 0;
    virtual void get_output_dimensions(
        const ProcessorConfig &proc_cfg,
        int in_width,
        int in_height,
        int &width,
        int &height
    ) const = 0;
};

// Abstract base class for filters
class Filter : public Processor {
   public:
    ProcessingMode get_processing_mode() const override { return ProcessingMode::Filter; }
    virtual int filter(AVFrame *in_frame, AVFrame **out_frame) = 0;
};

// Abstract base class for interpolators
class Interpolator : public Processor {
   public:
    ProcessingMode get_processing_mode() const override { return ProcessingMode::Interpolate; }
    virtual int
    interpolate(AVFrame *prev_frame, AVFrame *in_frame, AVFrame **out_frame, float time_step) = 0;
};

#endif  // PROCESSOR_H
