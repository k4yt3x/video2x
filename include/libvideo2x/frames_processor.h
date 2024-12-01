#ifndef FRAMES_PROCESSOR_H
#define FRAMES_PROCESSOR_H

#include "decoder.h"
#include "encoder.h"
#include "libvideo2x.h"
#include "processor.h"

int process_frames(
    const EncoderConfig *encoder_config,
    const ProcessorConfig *processor_config,
    VideoProcessingContext *proc_ctx,
    Decoder &decoder,
    Encoder &encoder,
    Processor *processor,
    bool benchmark = false
);

#endif  // FRAMES_PROCESSOR_H
