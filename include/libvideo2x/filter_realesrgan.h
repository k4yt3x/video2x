#ifndef FILTER_REALESRGAN_H
#define FILTER_REALESRGAN_H

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "char_defs.h"
#include "processor.h"
#include "realesrgan.h"

// FilterRealesrgan class definition
class FilterRealesrgan : public Filter {
   private:
    RealESRGAN *realesrgan_;
    int gpuid_;
    bool tta_mode_;
    int scaling_factor_;
    const StringType model_name_;
    AVRational in_time_base_;
    AVRational out_time_base_;
    AVPixelFormat out_pix_fmt_;

   public:
    // Constructor
    FilterRealesrgan(
        int gpuid = 0,
        bool tta_mode = false,
        int scaling_factor = 4,
        const StringType model_name = STR("realesr-animevideov3")
    );

    // Destructor
    virtual ~FilterRealesrgan() override;

    // Initializes the filter with decoder and encoder contexts
    int init(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx, AVBufferRef *hw_ctx) override;

    // Processes an input frame and returns the processed frame
    int filter(AVFrame *in_frame, AVFrame **out_frame) override;

    // Returns the filter's type
    ProcessorType get_processor_type() const override { return PROCESSOR_REALESRGAN; }
};

#endif  // FILTER_REALESRGAN_H
