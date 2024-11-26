#ifndef INTERPOLATOR_RIFE_H
#define INTERPOLATOR_RIFE_H

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "char_defs.h"
#include "filter.h"
#include "realesrgan.h"

// InterpolatorRIFE class definition
class InterpolatorRIFE : public Interpolator {
   private:
    RealESRGAN *realesrgan;
    int gpuid;
    bool tta_mode;
    int scaling_factor;
    const StringType model_name;
    AVRational in_time_base;
    AVRational out_time_base;
    AVPixelFormat out_pix_fmt;

   public:
    // Constructor
    InterpolatorRIFE(
        int gpuid = 0,
        bool tta_mode = false,
        bool tta_temporal_mode = false,
        bool uhd_mode = false,
        int num_threads = 1,
        bool rife_v2 = false,
        bool rife_v4 = true,
        const StringType model_name = STR("rife-v4.6"),
        float time_step = 0.5f
    );

    // Destructor
    virtual ~InterpolatorRIFE() override;

    // Initializes the filter with decoder and encoder contexts
    int init(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx, AVBufferRef *hw_ctx) override;

    // Processes an input frame and returns the processed frame
    int interpolate(AVFrame *in_frame, AVFrame *prev_frame, AVFrame **out_frame) override;
};

#endif  // INTERPOLATOR_RIFE_H
