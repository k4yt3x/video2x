#ifndef REALSRGAN_FILTER_H
#define REALSRGAN_FILTER_H

#include "filter.h"
#include "realesrgan.h"

// RealesrganFilter class definition
class RealesrganFilter : public Filter {
   private:
    RealESRGAN *realesrgan;
    int gpuid;
    bool tta_mode;
    int scaling_factor;
    const char *model;
    const char *custom_model_param_path;
    const char *custom_model_bin_path;
    AVRational input_time_base;
    AVRational output_time_base;
    AVPixelFormat output_pix_fmt;

   public:
    // Constructor
    RealesrganFilter(
        int gpuid = 0,
        bool tta_mode = false,
        int scaling_factor = 4,
        const char *model = "realesr-animevideov3",
        const char *custom_model_bin_pathmodel_param_path = nullptr,
        const char *custom_model_bin_pathmodel_bin_path = nullptr
    );

    // Destructor
    virtual ~RealesrganFilter();

    // Initializes the filter with decoder and encoder contexts
    int init(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx) override;

    // Processes an input frame and returns the processed frame
    AVFrame *process_frame(AVFrame *input_frame) override;

    // Flushes any remaining frames (if necessary)
    int flush(std::vector<AVFrame *> &processed_frames) override;
};

#endif
