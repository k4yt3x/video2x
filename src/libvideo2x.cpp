#include "libvideo2x.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include <libavutil/avutil.h>
}

#include <spdlog/spdlog.h>

#include "decoder.h"
#include "encoder.h"
#include "frames_processor.h"
#include "processor.h"
#include "processor_factory.h"

static void set_log_level(Libvideo2xLogLevel log_level) {
    switch (log_level) {
        case LIBVIDEO2X_LOG_LEVEL_TRACE:
            av_log_set_level(AV_LOG_TRACE);
            spdlog::set_level(spdlog::level::trace);
            break;
        case LIBVIDEO2X_LOG_LEVEL_DEBUG:
            av_log_set_level(AV_LOG_DEBUG);
            spdlog::set_level(spdlog::level::debug);
            break;
        case LIBVIDEO2X_LOG_LEVEL_INFO:
            av_log_set_level(AV_LOG_INFO);
            spdlog::set_level(spdlog::level::info);
            break;
        case LIBVIDEO2X_LOG_LEVEL_WARNING:
            av_log_set_level(AV_LOG_WARNING);
            spdlog::set_level(spdlog::level::warn);
            break;
        case LIBVIDEO2X_LOG_LEVEL_ERROR:
            av_log_set_level(AV_LOG_ERROR);
            spdlog::set_level(spdlog::level::err);
            break;
        case LIBVIDEO2X_LOG_LEVEL_CRITICAL:
            av_log_set_level(AV_LOG_FATAL);
            spdlog::set_level(spdlog::level::critical);
            break;
        case LIBVIDEO2X_LOG_LEVEL_OFF:
            av_log_set_level(AV_LOG_QUIET);
            spdlog::set_level(spdlog::level::off);
            break;
        default:
            av_log_set_level(AV_LOG_INFO);
            spdlog::set_level(spdlog::level::info);
            break;
    }
}

extern "C" int process_video(
    const CharType *in_fname,
    const CharType *out_fname,
    Libvideo2xLogLevel log_level,
    bool benchmark,
    uint32_t vk_device_index,
    AVHWDeviceType hw_type,
    const ProcessorConfig *processor_config,
    EncoderConfig *encoder_config,
    VideoProcessingContext *proc_ctx
) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    int ret = 0;

    // Set the log level for FFmpeg and spdlog
    set_log_level(log_level);

    // Convert the file names to std::filesystem::path
    std::filesystem::path in_fpath(in_fname);
    std::filesystem::path out_fpath(out_fname);

    // Create a smart pointer to manage the hardware device context
    auto hw_ctx_deleter = [](AVBufferRef *ref) {
        if (ref != nullptr) {
            av_buffer_unref(&ref);
        }
    };
    std::unique_ptr<AVBufferRef, decltype(hw_ctx_deleter)> hw_ctx(nullptr, hw_ctx_deleter);

    // Initialize hardware device context
    if (hw_type != AV_HWDEVICE_TYPE_NONE) {
        AVBufferRef *tmp_hw_ctx = nullptr;
        ret = av_hwdevice_ctx_create(&tmp_hw_ctx, hw_type, NULL, NULL, 0);
        if (ret < 0) {
            av_strerror(ret, errbuf, sizeof(errbuf));
            spdlog::critical("Error initializing hardware device context: {}", errbuf);
            return ret;
        }
        hw_ctx.reset(tmp_hw_ctx);
    }

    // Initialize input decoder
    Decoder decoder;
    ret = decoder.init(hw_type, hw_ctx.get(), in_fpath);
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
        ProcessorFactory::instance().create_processor(processor_config, vk_device_index)
    );
    if (processor == nullptr) {
        spdlog::critical("Failed to create filter instance");
        return -1;
    }

    // Initialize output dimensions based on filter configuration
    int output_width = 0, output_height = 0;
    processor->get_output_dimensions(
        processor_config, dec_ctx->width, dec_ctx->height, output_width, output_height
    );
    if (output_width <= 0 || output_height <= 0) {
        spdlog::critical("Failed to determine the output dimensions");
        return -1;
    }

    // Update encoder configuration with output dimensions
    encoder_config->width = output_width;
    encoder_config->height = output_height;

    // Initialize the encoder
    Encoder encoder;
    ret = encoder.init(
        hw_ctx.get(), out_fpath, ifmt_ctx, dec_ctx, encoder_config, processor_config, in_vstream_idx
    );
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
    ret = process_frames(
        encoder_config, processor_config, proc_ctx, decoder, encoder, processor.get(), benchmark
    );
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
