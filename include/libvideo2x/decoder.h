#pragma once

#include <filesystem>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace video2x {
namespace decoder {

class Decoder {
   public:
    Decoder();
    ~Decoder();

    int init(AVHWDeviceType hw_type, AVBufferRef* hw_ctx, const std::filesystem::path& in_fpath);

    AVFormatContext* get_format_context() const;
    AVCodecContext* get_codec_context() const;
    int get_video_stream_index() const;

   private:
    static AVPixelFormat hw_pix_fmt_;
    static AVPixelFormat get_hw_format(AVCodecContext* ctx, const AVPixelFormat* pix_fmts);

    AVFormatContext* fmt_ctx_;
    AVCodecContext* dec_ctx_;
    int in_vstream_idx_;
};

}  // namespace decoder
}  // namespace video2x
