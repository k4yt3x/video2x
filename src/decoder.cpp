#include "decoder.h"

#include <spdlog/spdlog.h>

#include "logger_manager.h"

namespace video2x {
namespace decoder {

AVPixelFormat Decoder::hw_pix_fmt_ = AV_PIX_FMT_NONE;

Decoder::Decoder() : fmt_ctx_(nullptr), dec_ctx_(nullptr), in_vstream_idx_(-1) {}

Decoder::~Decoder() {
    if (dec_ctx_) {
        avcodec_free_context(&dec_ctx_);
        dec_ctx_ = nullptr;
    }
    if (fmt_ctx_) {
        avformat_close_input(&fmt_ctx_);
        fmt_ctx_ = nullptr;
    }
}

AVPixelFormat Decoder::get_hw_format(AVCodecContext*, const AVPixelFormat* pix_fmts) {
    for (const AVPixelFormat* p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
        if (*p == hw_pix_fmt_) {
            return *p;
        }
    }
    logger()->error("Failed to get HW surface format.");
    return AV_PIX_FMT_NONE;
}

int Decoder::init(
    AVHWDeviceType hw_type,
    AVBufferRef* hw_ctx,
    const std::filesystem::path& in_fpath
) {
    int ret;

    // Open the input file
    if ((ret = avformat_open_input(&fmt_ctx_, in_fpath.u8string().c_str(), nullptr, nullptr)) < 0) {
        logger()->error("Could not open input file '{}'", in_fpath.u8string());
        return ret;
    }

    // Retrieve stream information
    if ((ret = avformat_find_stream_info(fmt_ctx_, nullptr)) < 0) {
        logger()->error("Failed to retrieve input stream information");
        return ret;
    }

    // Find the first video stream
    ret = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (ret < 0) {
        logger()->error("Could not find video stream in the input file");
        return ret;
    }

    int stream_index = ret;
    AVStream* video_stream = fmt_ctx_->streams[stream_index];

    // Find the decoder for the video stream
    const AVCodec* decoder = avcodec_find_decoder(video_stream->codecpar->codec_id);
    if (!decoder) {
        logger()->error(
            "Failed to find decoder for codec ID {}",
            static_cast<int>(video_stream->codecpar->codec_id)
        );
        return AVERROR_DECODER_NOT_FOUND;
    }

    // Allocate the decoder context
    dec_ctx_ = avcodec_alloc_context3(decoder);
    if (!dec_ctx_) {
        logger()->error("Failed to allocate the decoder context");
        return AVERROR(ENOMEM);
    }

    // Copy codec parameters from input stream to decoder context
    if ((ret = avcodec_parameters_to_context(dec_ctx_, video_stream->codecpar)) < 0) {
        logger()->error("Failed to copy decoder parameters to input decoder context");
        return ret;
    }

    // Set the time base and frame rate
    dec_ctx_->time_base = video_stream->time_base;
    dec_ctx_->pkt_timebase = video_stream->time_base;
    dec_ctx_->framerate = av_guess_frame_rate(fmt_ctx_, video_stream, nullptr);

    // Set hardware device context
    if (hw_ctx != nullptr) {
        dec_ctx_->hw_device_ctx = av_buffer_ref(hw_ctx);
        dec_ctx_->get_format = get_hw_format;

        // Automatically determine the hardware pixel format
        for (int i = 0;; i++) {
            const AVCodecHWConfig* config = avcodec_get_hw_config(decoder, i);
            if (config == nullptr) {
                logger()->error(
                    "Decoder {} does not support device type {}.",
                    decoder->name,
                    av_hwdevice_get_type_name(hw_type)
                );
                return AVERROR(ENOSYS);
            }
            if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
                config->device_type == hw_type) {
                hw_pix_fmt_ = config->pix_fmt;
                break;
            }
        }
    }

    // Open the decoder
    if ((ret = avcodec_open2(dec_ctx_, decoder, nullptr)) < 0) {
        logger()->error("Failed to open decoder for stream #{}", stream_index);
        return ret;
    }

    in_vstream_idx_ = stream_index;

    return 0;
}

AVFormatContext* Decoder::get_format_context() const {
    return fmt_ctx_;
}

AVCodecContext* Decoder::get_codec_context() const {
    return dec_ctx_;
}

int Decoder::get_video_stream_index() const {
    return in_vstream_idx_;
}

}  // namespace decoder
}  // namespace video2x
