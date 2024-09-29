#ifndef LIBVIDEO2X_H
#define LIBVIDEO2X_H

int process_video(
    const char *input_filename,
    const char *output_filename,
    const char *shader_path,
    int output_width,
    int output_height
);

#endif  // LIBVIDEO2X_H
