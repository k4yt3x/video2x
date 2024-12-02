#include "libvideo2x.h"

extern "C" {
#include <libavutil/avutil.h>
}

#include <spdlog/spdlog.h>

#include "decoder.h"
#include "encoder.h"
#include "frames_processor.h"
#include "processor.h"
#include "processor_factory.h"

int process_video(
    const std::filesystem::path in_fname,
    const std::filesystem::path out_fname,
    const HardwareConfig hw_cfg,
    const ProcessorConfig proc_cfg,
    EncoderConfig enc_cfg,
    VideoProcessingContext *proc_ctx,
    Libvideo2xLogLevel log_level,
    bool benchmark
) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    int ret = 0;

    // Set the log level for FFmpeg and spdlog
    set_log_level(log_level);

    // Create a smart pointer to manage the hardware device context
    auto hw_ctx_deleter = [](AVBufferRef *ref) {
        if (ref != nullptr) {
            av_buffer_unref(&ref);
        }
    };
    std::unique_ptr<AVBufferRef, decltype(hw_ctx_deleter)> hw_ctx(nullptr, hw_ctx_deleter);

    // Initialize hardware device context
    if (hw_cfg.hw_device_type != AV_HWDEVICE_TYPE_NONE) {
        AVBufferRef *tmp_hw_ctx = nullptr;
        ret = av_hwdevice_ctx_create(&tmp_hw_ctx, hw_cfg.hw_device_type, NULL, NULL, 0);
        if (ret < 0) {
            av_strerror(ret, errbuf, sizeof(errbuf));
            spdlog::critical("Error initializing hardware device context: {}", errbuf);
            return ret;
        }
        hw_ctx.reset(tmp_hw_ctx);
    }

    // Initialize input decoder
    Decoder decoder;
    ret = decoder.init(hw_cfg.hw_device_type, hw_ctx.get(), in_fname);
    if (ret < 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        spdlog::critical("Failed to initialize decoder: {}", errbuf);
        return ret;
    }

    AVFormatContext *ifmt_ctx = decoder.get_format_context();
    AVCodecContext *dec_ctx = decoder.get_codec_context();
    int in_vstream_idx = decoder.get_video_stream_index();

    // Create and initialize the appropriate filter
    std::unique_ptr<Processor> processor(
        ProcessorFactory::instance().create_processor(proc_cfg, hw_cfg.vk_device_index)
    );
    if (processor == nullptr) {
        spdlog::critical("Failed to create filter instance");
        return -1;
    }

    // Initialize output dimensions based on filter configuration
    int output_width = 0, output_height = 0;
    processor->get_output_dimensions(
        proc_cfg, dec_ctx->width, dec_ctx->height, output_width, output_height
    );
    if (output_width <= 0 || output_height <= 0) {
        spdlog::critical("Failed to determine the output dimensions");
        return -1;
    }

    // Update encoder configuration with output dimensions
    enc_cfg.width = output_width;
    enc_cfg.height = output_height;

    // Initialize the encoder
    Encoder encoder;
    ret =
        encoder.init(hw_ctx.get(), out_fname, ifmt_ctx, dec_ctx, enc_cfg, proc_cfg, in_vstream_idx);
    if (ret < 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        spdlog::critical("Failed to initialize encoder: {}", errbuf);
        return ret;
    }

    // Initialize the filter
    ret = processor->init(dec_ctx, encoder.get_encoder_context(), hw_ctx.get());
    if (ret < 0) {
        spdlog::critical("Failed to initialize filter");
        return ret;
    }

    // Process frames using the encoder and decoder
    ret = process_frames(enc_cfg, proc_cfg, proc_ctx, decoder, encoder, processor.get(), benchmark);
    if (ret < 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        spdlog::critical("Error processing frames: {}", errbuf);
        return ret;
    }

    // Write the output file trailer
    av_write_trailer(encoder.get_format_context());

    if (ret < 0 && ret != AVERROR_EOF) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        spdlog::critical("Error occurred: {}", errbuf);
        return ret;
    }
    return 0;
}
