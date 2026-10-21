#include "filter_coreml.h"

#import <CoreML/CoreML.h>
#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>

#include <cstring>

#include <spdlog/spdlog.h>

#include "conversions.h"
#include "logger_manager.h"

namespace video2x {
namespace processors {

struct FilterCoreML::Impl {
    MLModel* model = nil;
};

FilterCoreML::FilterCoreML(int scaling_factor, std::filesystem::path model_path)
    : impl_(std::make_unique<Impl>()),
      scaling_factor_(scaling_factor),
      model_path_(std::move(model_path)) {}

FilterCoreML::~FilterCoreML() = default;

int FilterCoreML::init(AVCodecContext* dec_ctx, AVCodecContext* enc_ctx, AVBufferRef*) {
    in_time_base_ = dec_ctx->time_base;
    out_time_base_ = enc_ctx->time_base;
    out_pix_fmt_ = enc_ctx->pix_fmt;

    // Prefer a fixed-shape model matching the video's resolution; it is much faster
    std::filesystem::path pkg = model_path_;
    std::error_code ec;
    std::filesystem::path fixed = model_path_;
    fixed.replace_filename(
        model_path_.stem().string() + "-" + std::to_string(dec_ctx->width) + "x" +
        std::to_string(dec_ctx->height) + ".mlpackage"
    );
    if (std::filesystem::exists(fixed, ec)) {
        pkg = fixed;
    }
    logger()->info("Core ML model: {}", pkg.filename().string());

    NSError* err = nil;
    NSURL* url = [NSURL fileURLWithPath:@(pkg.c_str())];

    // Core ML can only load compiled models; compile once and cache next to the package
    std::filesystem::path compiled = pkg;
    compiled.replace_extension(".mlmodelc");
    if (std::filesystem::exists(compiled, ec) &&
        std::filesystem::last_write_time(compiled, ec) >=
            std::filesystem::last_write_time(pkg, ec)) {
        url = [NSURL fileURLWithPath:@(compiled.c_str())];
    } else {
        logger()->info("Compiling Core ML model (first run only)");
        NSURL* tmp = [MLModel compileModelAtURL:url error:&err];
        if (tmp == nil) {
            logger()->error("Failed to compile Core ML model: {}", err.localizedDescription.UTF8String);
            return -1;
        }
        NSURL* dst = [NSURL fileURLWithPath:@(compiled.c_str())];
        NSFileManager* fm = [NSFileManager defaultManager];
        [fm removeItemAtURL:dst error:nil];
        url = [fm copyItemAtURL:tmp toURL:dst error:nil] ? dst : tmp;
    }

    MLModelConfiguration* cfg = [[MLModelConfiguration alloc] init];
    // The Neural Engine measured slower than the GPU for this network
    cfg.computeUnits = MLComputeUnitsCPUAndGPU;
    impl_->model = [MLModel modelWithContentsOfURL:url configuration:cfg error:&err];
    if (impl_->model == nil) {
        logger()->error("Failed to load Core ML model: {}", err.localizedDescription.UTF8String);
        return -1;
    }
    return 0;
}

static void copy_rows(
    const uint8_t* src, size_t src_stride, uint8_t* dst, size_t dst_stride, size_t row_bytes, int h
) {
    for (int y = 0; y < h; y++) {
        std::memcpy(dst + y * dst_stride, src + y * src_stride, row_bytes);
    }
}

int FilterCoreML::filter(AVFrame* in_frame, AVFrame** out_frame) {
    @autoreleasepool {
        AVFrame* in_bgra = conversions::convert_avframe_pix_fmt(in_frame, AV_PIX_FMT_BGRA);
        if (in_bgra == nullptr) {
            return -1;
        }
        const int w = in_bgra->width;
        const int h = in_bgra->height;

        CVPixelBufferRef in_pb = nullptr;
        NSDictionary* attrs = @{(id)kCVPixelBufferIOSurfacePropertiesKey : @{}};
        CVReturn cv = CVPixelBufferCreate(
            kCFAllocatorDefault, static_cast<size_t>(w), static_cast<size_t>(h),
            kCVPixelFormatType_32BGRA, (__bridge CFDictionaryRef)attrs, &in_pb
        );
        if (cv != kCVReturnSuccess) {
            av_frame_free(&in_bgra);
            logger()->error("Failed to create input pixel buffer");
            return -1;
        }
        CVPixelBufferLockBaseAddress(in_pb, 0);
        copy_rows(
            in_bgra->data[0], static_cast<size_t>(in_bgra->linesize[0]),
            static_cast<uint8_t*>(CVPixelBufferGetBaseAddress(in_pb)),
            CVPixelBufferGetBytesPerRow(in_pb), static_cast<size_t>(w) * 4, h
        );
        CVPixelBufferUnlockBaseAddress(in_pb, 0);
        av_frame_free(&in_bgra);

        NSError* err = nil;
        MLDictionaryFeatureProvider* input = [[MLDictionaryFeatureProvider alloc]
            initWithDictionary:@{@"x" : [MLFeatureValue featureValueWithPixelBuffer:in_pb]}
                         error:&err];
        id<MLFeatureProvider> output =
            input ? [impl_->model predictionFromFeatures:input error:&err] : nil;
        CFRelease(in_pb);
        CVPixelBufferRef out_pb = [output featureValueForName:@"y"].imageBufferValue;
        if (out_pb == nullptr) {
            logger()->error("Core ML prediction failed: {}", err.localizedDescription.UTF8String);
            return -1;
        }
        if (CVPixelBufferGetPixelFormatType(out_pb) != kCVPixelFormatType_32BGRA ||
            CVPixelBufferGetWidth(out_pb) != static_cast<size_t>(w * scaling_factor_)) {
            logger()->error("Unexpected Core ML output (wrong pixel format or scale)");
            return -1;
        }

        AVFrame* out_bgra = av_frame_alloc();
        out_bgra->format = AV_PIX_FMT_BGRA;
        out_bgra->width = w * scaling_factor_;
        out_bgra->height = h * scaling_factor_;
        if (av_frame_get_buffer(out_bgra, 32) < 0) {
            av_frame_free(&out_bgra);
            return AVERROR(ENOMEM);
        }
        CVPixelBufferLockBaseAddress(out_pb, kCVPixelBufferLock_ReadOnly);
        copy_rows(
            static_cast<const uint8_t*>(CVPixelBufferGetBaseAddress(out_pb)),
            CVPixelBufferGetBytesPerRow(out_pb), out_bgra->data[0],
            static_cast<size_t>(out_bgra->linesize[0]), static_cast<size_t>(out_bgra->width) * 4,
            out_bgra->height
        );
        CVPixelBufferUnlockBaseAddress(out_pb, kCVPixelBufferLock_ReadOnly);

        *out_frame = conversions::convert_avframe_pix_fmt(out_bgra, out_pix_fmt_);
        av_frame_free(&out_bgra);
        if (*out_frame == nullptr) {
            return AVERROR(ENOMEM);
        }
        (*out_frame)->pts = av_rescale_q(in_frame->pts, in_time_base_, out_time_base_);
        return 0;
    }
}

void FilterCoreML::get_output_dimensions(
    const ProcessorConfig&, int in_width, int in_height, int& out_width, int& out_height
) const {
    out_width = in_width * scaling_factor_;
    out_height = in_height * scaling_factor_;
}

}  // namespace processors
}  // namespace video2x
