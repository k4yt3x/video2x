#ifndef LIBVIDEO2X_H
#define LIBVIDEO2X_H

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
#include "logging.h"
#include "processor.h"

#ifdef _WIN32
#ifdef LIBVIDEO2X_EXPORTS
#define LIBVIDEO2X_API __declspec(dllexport)
#else
#define LIBVIDEO2X_API __declspec(dllimport)
#endif
#else
#define LIBVIDEO2X_API
#endif

struct HardwareConfig {
    uint32_t vk_device_index;
    AVHWDeviceType hw_device_type;
};

class LIBVIDEO2X_API VideoProcessor {
   public:
    VideoProcessor(
        const HardwareConfig hw_cfg,
        const ProcessorConfig proc_cfg,
        EncoderConfig enc_cfg,
        Video2xLogLevel = Video2xLogLevel::Info,
        bool benchmark = false
    );

    virtual ~VideoProcessor() = default;

    [[nodiscard]] int
    process(const std::filesystem::path in_fname, const std::filesystem::path out_fname);

    void pause() { paused_.store(true); }
    void resume() { paused_.store(false); }
    void abort() { aborted_.store(true); }

    int64_t get_processed_frames() const { return frame_index_.load(); }
    int64_t get_total_frames() const { return total_frames_.load(); }

    bool is_paused() const { return paused_.load(); }
    bool is_aborted() const { return aborted_.load(); }
    bool is_completed() const { return completed_.load(); }

   private:
    [[nodiscard]] int
    process_frames(Decoder &decoder, Encoder &encoder, std::unique_ptr<Processor> &processor);

    [[nodiscard]] int write_frame(AVFrame *frame, Encoder &encoder);

    [[nodiscard]] inline int write_raw_packet(
        AVPacket *packet,
        AVFormatContext *ifmt_ctx,
        AVFormatContext *ofmt_ctx,
        int *stream_map
    );

    [[nodiscard]] inline int process_filtering(
        std::unique_ptr<Processor> &processor,
        Encoder &encoder,
        AVFrame *frame,
        AVFrame *proc_frame
    );

    [[nodiscard]] inline int process_interpolation(
        std::unique_ptr<Processor> &processor,
        Encoder &encoder,
        std::unique_ptr<AVFrame, decltype(&av_frame_deleter)> &prev_frame,
        AVFrame *frame,
        AVFrame *proc_frame
    );

    HardwareConfig hw_cfg_;
    ProcessorConfig proc_cfg_;
    EncoderConfig enc_cfg_;
    bool benchmark_ = false;

    std::atomic<int64_t> frame_index_ = 0;
    std::atomic<int64_t> total_frames_ = 0;
    std::atomic<bool> paused_ = false;
    std::atomic<bool> aborted_ = false;
    std::atomic<bool> completed_ = false;
};

#endif  // LIBVIDEO2X_H
