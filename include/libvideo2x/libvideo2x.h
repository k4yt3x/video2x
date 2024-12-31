#pragma once

#include <atomic>
#include <cstdint>
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "avutils.h"
#include "decoder.h"
#include "encoder.h"
#include "libvideo2x_export.h"
#include "processor.h"

namespace video2x {

enum class VideoProcessorState {
    Idle,
    Running,
    Paused,
    Failed,
    Aborted,
    Completed
};

class LIBVIDEO2X_API VideoProcessor {
   public:
    VideoProcessor(
        const processors::ProcessorConfig proc_cfg,
        const encoder::EncoderConfig enc_cfg,
        const uint32_t vk_device_idx = 0,
        const AVHWDeviceType hw_device_type = AV_HWDEVICE_TYPE_NONE,
        const bool benchmark = false
    );

    virtual ~VideoProcessor() = default;

    [[nodiscard]] int
    process(const std::filesystem::path in_fname, const std::filesystem::path out_fname);

    void pause() { state_.store(VideoProcessorState::Paused); }
    void resume() { state_.store(VideoProcessorState::Running); }
    void abort() { state_.store(VideoProcessorState::Aborted); }

    VideoProcessorState get_state() const { return state_.load(); }
    int64_t get_processed_frames() const { return frame_idx_.load(); }
    int64_t get_total_frames() const { return total_frames_.load(); }

   private:
    [[nodiscard]] int process_frames(
        decoder::Decoder& decoder,
        encoder::Encoder& encoder,
        std::unique_ptr<processors::Processor>& processor
    );

    [[nodiscard]] int write_frame(AVFrame* frame, encoder::Encoder& encoder);

    [[nodiscard]] inline int write_raw_packet(
        AVPacket* packet,
        AVFormatContext* ifmt_ctx,
        AVFormatContext* ofmt_ctx,
        int* stream_map
    );

    [[nodiscard]] inline int process_filtering(
        std::unique_ptr<processors::Processor>& processor,
        encoder::Encoder& encoder,
        AVFrame* frame,
        AVFrame* proc_frame
    );

    [[nodiscard]] inline int process_interpolation(
        std::unique_ptr<processors::Processor>& processor,
        encoder::Encoder& encoder,
        std::unique_ptr<AVFrame, decltype(&avutils::av_frame_deleter)>& prev_frame,
        AVFrame* frame,
        AVFrame* proc_frame
    );

    processors::ProcessorConfig proc_cfg_;
    encoder::EncoderConfig enc_cfg_;
    uint32_t vk_device_idx_ = 0;
    AVHWDeviceType hw_device_type_ = AV_HWDEVICE_TYPE_NONE;
    bool benchmark_ = false;

    std::atomic<VideoProcessorState> state_ = VideoProcessorState::Idle;
    std::atomic<int64_t> frame_idx_ = 0;
    std::atomic<int64_t> total_frames_ = 0;
};

}  // namespace video2x
