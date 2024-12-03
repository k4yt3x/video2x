#ifndef ENCODER_H
#define ENCODER_H

#include <cstdint>
#include <filesystem>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
}

#include "fsutils.h"

// Encoder configurations
struct EncoderConfig {
    // Non-AVCodecContext options
    AVCodecID codec;
    bool copy_streams;

    // Basic video options
    int width;
    int height;
    int frm_rate_mul;
    AVPixelFormat pix_fmt;

    // Rate control and compression
    int64_t bit_rate;
    int rc_buffer_size;
    int rc_min_rate;
    int rc_max_rate;
    int qmin;
    int qmax;

    // GOP and frame structure
    int gop_size;
    int max_b_frames;
    int keyint_min;
    int refs;

    // Performance and threading
    int thread_count;

    // Latency and buffering
    int delay;

    // Extra AVOptions
    std::vector<std::pair<StringType, StringType>> extra_opts;
};

class Encoder {
   public:
    Encoder();
    ~Encoder();

    int init(
        AVBufferRef *hw_ctx,
        const std::filesystem::path &out_fpath,
        AVFormatContext *ifmt_ctx,
        AVCodecContext *dec_ctx,
        EncoderConfig &enc_cfg,
        int in_vstream_idx
    );

    int write_frame(AVFrame *frame, int64_t frame_idx);
    int flush();

    AVCodecContext *get_encoder_context() const;
    AVFormatContext *get_format_context() const;
    int *get_stream_map() const;
    int get_output_video_stream_index() const;

   private:
    AVFormatContext *ofmt_ctx_;
    AVCodecContext *enc_ctx_;
    int out_vstream_idx_;
    int *stream_map_;
};

#endif  // ENCODER_H
