#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libvideo2x.h>

int main(int argc, char **argv) {
    if (argc < 6) {
        fprintf(
            stderr,
            "Usage: %s <input_video> <output_video> <shader.glsl> <output_width> <output_height> <filter_type>\n",
            argv[0]
        );
        return 1;
    }

    // Parse arguments
    const char *input_filename = argv[1];
    const char *output_filename = argv[2];
    int output_width = atoi(argv[3]);
    int output_height = atoi(argv[4]);
    const char *filter_type_str = argv[5];

    const char *shader_path = nullptr;
    if (argc > 6) {
        shader_path = argv[6];
    }

    FilterType filter_type;
    if (strcmp(filter_type_str, "libplacebo") == 0) {
        filter_type = FILTER_LIBPLACEBO;
    } else if (strcmp(filter_type_str, "realesrgan") == 0) {
        filter_type = FILTER_REALESRGAN;
    } else {
        fprintf(stderr, "Invalid filter type.\n");
        return 1;
    }

    if (process_video(
            input_filename, output_filename, output_width, output_height, filter_type, shader_path
        )) {
        fprintf(stderr, "Video processing failed.\n");
        return 1;
    }

    return 0;
}
