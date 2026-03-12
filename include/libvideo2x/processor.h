#pragma once

#include <variant>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavutil/buffer.h>
#include <libavutil/pixdesc.h>
}

#include "fsutils.h"
#include "logger_manager.h"

namespace video2x {
namespace processors {

enum class ProcessingMode {
    Filter,
    Interpolate,
};

enum class ProcessorType {
    None,
    Libplacebo,
    RealESRGAN,
    RealCUGAN,
    RIFE,
};

struct LibplaceboConfig {
    fsutils::StringType shader_path;
};

struct RealESRGANConfig {
    bool tta_mode = false;
    fsutils::StringType model_name;
};

struct RealCUGANConfig {
    bool tta_mode = false;
    int num_threads = 1;
    int syncgap = 3;
    fsutils::StringType model_name;
};

struct RIFEConfig {
    bool tta_mode = false;
    bool tta_temporal_mode = false;
    bool uhd_mode = false;
    int num_threads = 0;
    fsutils::StringType model_name;
};

// Unified filter configuration
struct ProcessorConfig {
    ProcessorType processor_type = ProcessorType::None;
    int width = 0;
    int height = 0;
    int scaling_factor = 0;
    int noise_level = -1;
    int frm_rate_mul = 0;
    float scn_det_thresh = 0.0f;
    std::variant<LibplaceboConfig, RealESRGANConfig, RealCUGANConfig, RIFEConfig> config;
};

class Processor {
   public:
    virtual ~Processor() = default;
    virtual int init(AVCodecContext* dec_ctx, AVCodecContext* enc_ctx, AVBufferRef* hw_ctx) = 0;
    virtual int flush(std::vector<AVFrame*>&) { return 0; }
    virtual ProcessingMode get_processing_mode() const = 0;
    virtual ProcessorType get_processor_type() const = 0;
    virtual void get_output_dimensions(
        const ProcessorConfig& proc_cfg,
        int in_width,
        int in_height,
        int& width,
        int& height
    ) const = 0;

   protected:
    // Authoritative color properties captured from dec_ctx at init time.
    // Per-frame AVFrame fields are often AVCOL_*_UNSPECIFIED even when the
    // stream header carries valid values, so we store them here once.
    AVColorSpace out_color_space_ = AVCOL_SPC_UNSPECIFIED;
    AVColorRange out_color_range_ = AVCOL_RANGE_UNSPECIFIED;
    AVColorPrimaries out_color_primaries_ = AVCOL_PRI_UNSPECIFIED;
    AVColorTransferCharacteristic out_color_trc_ = AVCOL_TRC_UNSPECIFIED;

    // Assign all four color properties from dec_ctx and log them at info level.
    // Call once at the end of init() in each subclass.
    void capture_color_properties(AVCodecContext* dec_ctx) {
        out_color_space_ = dec_ctx->colorspace;
        out_color_range_ = dec_ctx->color_range;
        out_color_primaries_ = dec_ctx->color_primaries;
        out_color_trc_ = dec_ctx->color_trc;
        logger()->info(
            "Decoder color properties: space={}, range={}, primaries={}, trc={}",
            av_color_space_name(out_color_space_),
            av_color_range_name(out_color_range_),
            av_color_primaries_name(out_color_primaries_),
            av_color_transfer_name(out_color_trc_)
        );
    }

    // Build a lightweight AVFrame carrying only color metadata.
    // Pass the result as color_hint to avframe_to_ncnn_mat / ncnn_mat_to_avframe
    // so both conversion directions use the same, consistent color matrix.
    AVFrame make_color_hint() const {
        AVFrame color_hint = {};
        color_hint.colorspace = out_color_space_;
        color_hint.color_range = out_color_range_;
        color_hint.color_primaries = out_color_primaries_;
        color_hint.color_trc = out_color_trc_;
        return color_hint;
    }
};

// Abstract base class for filters
class Filter : public Processor {
   public:
    ProcessingMode get_processing_mode() const override { return ProcessingMode::Filter; }
    virtual int filter(AVFrame* in_frame, AVFrame** out_frame) = 0;
};

// Abstract base class for interpolators
class Interpolator : public Processor {
   public:
    ProcessingMode get_processing_mode() const override { return ProcessingMode::Interpolate; }
    virtual int
    interpolate(AVFrame* prev_frame, AVFrame* in_frame, AVFrame** out_frame, float time_step) = 0;

    // Normalize a pass-through frame so it goes through the same pixel-format
    // round-trip as interpolated frames, eliminating visual differences between
    // original and interpolated frames.
    // The default implementation performs the ncnn pixel-format round-trip and
    // rescales PTS to the encoder time base, which is correct for all ncnn-based
    // interpolators. Non-ncnn subclasses (e.g., CUDA-based) should override this
    // to match their own conversion pipeline.
    virtual int normalize(AVFrame* in_frame, AVFrame** out_frame);

   protected:
    AVRational in_time_base_ = {0, 1};
    AVRational out_time_base_ = {0, 1};
    AVPixelFormat out_pix_fmt_ = AV_PIX_FMT_NONE;
};

}  // namespace processors
}  // namespace video2x
