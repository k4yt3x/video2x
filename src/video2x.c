#include <stdio.h>
#include <stdlib.h>

#include <libvideo2x.h>

int main(int argc, char **argv) {
    if (argc != 6) {
        fprintf(
            stderr,
            "Usage: %s <input_video> <output_video> <shader.glsl> <output_width> <output_height>\n",
            argv[0]
        );
        return 1;
    }

    // Parse arguments
    const char *input_filename = argv[1];
    const char *output_filename = argv[2];
    const char *shader_path = argv[3];
    int output_width = atoi(argv[4]);
    int output_height = atoi(argv[5]);

    if (process_video(input_filename, output_filename, shader_path, output_width, output_height)) {
        fprintf(stderr, "Video processing failed.\n");
        return 1;
    }

    return 0;
}
