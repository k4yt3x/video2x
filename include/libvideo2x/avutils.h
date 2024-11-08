#ifndef AVUTILS_H
#define AVUTILS_H

extern "C" {
#include <libavformat/avformat.h>
}

int64_t get_video_frame_count(AVFormatContext *ifmt_ctx, int in_vstream_idx);

#endif  // AVUTILS_H
