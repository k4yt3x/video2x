#ifndef INTERPOLATOR_RIFE_H
#define INTERPOLATOR_RIFE_H

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "char_defs.h"
#include "processor.h"
#include "rife.h"

// InterpolatorRIFE class definition
class InterpolatorRIFE : public Interpolator {
   private:
    RIFE *rife_;
    int gpuid_;
    bool tta_mode_;
    bool tta_temporal_mode_;
    bool uhd_mode_;
    int num_threads_;
    bool rife_v2_;
    bool rife_v4_;
    const StringType model_name_;
    AVRational in_time_base_;
    AVRational out_time_base_;
    AVPixelFormat out_pix_fmt_;

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
        const StringType model_name = STR("rife-v4.6")
    );

    // Destructor
    virtual ~InterpolatorRIFE() override;

    // Initializes the interpolator with decoder and encoder contexts
    int init(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx, AVBufferRef *hw_ctx) override;

    // Processes an input frame and returns the processed frame
    int interpolate(AVFrame *prev_frame, AVFrame *in_frame, AVFrame **out_frame, float time_step)
        override;

    // Returns the interpolator's type
    ProcessorType get_processor_type() const override { return PROCESSOR_RIFE; }

    // Returns the interpolator's output dimensions
    void get_output_dimensions(
        const ProcessorConfig *processor_config,
        int in_width,
        int in_height,
        int &out_width,
        int &out_height
    ) const override;
};

#endif  // INTERPOLATOR_RIFE_H
