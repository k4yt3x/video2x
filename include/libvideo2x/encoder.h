#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
}

namespace video2x {
namespace encoder {

// Encoder configurations
struct EncoderConfig {
    // Non-AVCodecContext options
    std::string codec = "libx264";
    bool copy_streams = true;

    // Basic video options
    AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;

    // Rate control and compression
    int64_t bit_rate = 0;
    int rc_buffer_size = 0;
    int rc_min_rate = 0;
    int rc_max_rate = 0;
    int qmin = -1;
    int qmax = -1;

    // GOP and frame structure
    int gop_size = -1;
    int max_b_frames = -1;
    int keyint_min = -1;
    int refs = -1;

    // Performance and threading
    int thread_count = 0;

    // Latency and buffering
    int delay = -1;

    // Extra AVOptions
    std::vector<std::pair<std::string, std::string>> extra_opts;
};

class Encoder {
   public:
    Encoder();
    ~Encoder();

    int init(
        AVBufferRef* hw_ctx,
        const std::filesystem::path& out_fpath,
        AVFormatContext* ifmt_ctx,
        AVCodecContext* dec_ctx,
        EncoderConfig& enc_cfg,
        int width,
        int height,
        int frm_rate_mul,
        int in_vstream_idx
    );

    int write_frame(AVFrame* frame, int64_t frame_idx);
    int flush();

    AVCodecContext* get_encoder_context() const;
    AVFormatContext* get_format_context() const;
    int* get_stream_map() const;
    int get_output_video_stream_index() const;

   private:
    AVFormatContext* ofmt_ctx_;
    AVCodecContext* enc_ctx_;
    int out_vstream_idx_;
    int* stream_map_;
};

}  // namespace encoder
}  // namespace video2x
