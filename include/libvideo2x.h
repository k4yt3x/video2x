#ifndef LIBVIDEO2X_H
#define LIBVIDEO2X_H

enum FilterType {
    FILTER_LIBPLACEBO,
    FILTER_REALESRGAN
};

int process_video(
    const char *input_filename,
    const char *output_filename,
    int output_width,
    int output_height,
    FilterType filter_type,
    const char *shader_path
);

#endif  // LIBVIDEO2X_H
