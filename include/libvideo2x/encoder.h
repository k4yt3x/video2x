#ifndef ENCODER_H
#define ENCODER_H

#include <cstdint>
#include <filesystem>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
}

#include "libvideo2x/libvideo2x.h"

class Encoder {
   public:
    Encoder();
    ~Encoder();

    int init(
        AVBufferRef *hw_ctx,
        const std::filesystem::path &out_fpath,
        AVFormatContext *ifmt_ctx,
        AVCodecContext *dec_ctx,
        EncoderConfig *encoder_config,
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
