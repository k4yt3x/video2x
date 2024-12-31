#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "processor.h"
#include "realcugan.h"

namespace video2x {
namespace processors {

// FilterRealcugan class definition
class FilterRealcugan : public Filter {
   public:
    // Constructor
    FilterRealcugan(
        int gpuid = 0,
        bool tta_mode = false,
        int scaling_factor = 4,
        int noise_level = -1,
        int num_threads = 1,
        int syncgap = 3,
        const fsutils::StringType model_name = STR("models-pro")
    );

    // Destructor
    virtual ~FilterRealcugan() override;

    // Initializes the filter with decoder and encoder contexts
    int init(AVCodecContext* dec_ctx, AVCodecContext* enc_ctx, AVBufferRef* hw_ctx) override;

    // Processes an input frame and returns the processed frame
    int filter(AVFrame* in_frame, AVFrame** out_frame) override;

    // Returns the filter's type
    ProcessorType get_processor_type() const override { return ProcessorType::RealCUGAN; }

    // Returns the filter's output dimensions
    void get_output_dimensions(
        const ProcessorConfig& proc_cfg,
        int in_width,
        int in_height,
        int& out_width,
        int& out_height
    ) const override;

   private:
    RealCUGAN* realcugan_;
    int gpuid_;
    bool tta_mode_;
    int scaling_factor_;
    int noise_level_;
    int num_threads_;
    int syncgap_;
    const fsutils::StringType model_name_;
    AVRational in_time_base_;
    AVRational out_time_base_;
    AVPixelFormat out_pix_fmt_;
};

}  // namespace processors
}  // namespace video2x
