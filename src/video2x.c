#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>

#include <libvideo2x.h>

#include "getopt.h"

const char *VIDEO2X_VERSION = "6.0.0";

// Define command line options
static struct option long_options[] = {
    // General options
    {"input", required_argument, NULL, 'i'},
    {"output", required_argument, NULL, 'o'},
    {"filter", required_argument, NULL, 'f'},
    {"version", no_argument, NULL, 'v'},
    {"help", no_argument, NULL, 0},

    // Encoder options
    {"codec", required_argument, NULL, 'c'},
    {"preset", required_argument, NULL, 'p'},
    {"pixfmt", required_argument, NULL, 'x'},
    {"bitrate", required_argument, NULL, 'b'},
    {"crf", required_argument, NULL, 'q'},

    // Libplacebo options
    {"shader", required_argument, NULL, 's'},
    {"width", required_argument, NULL, 'w'},
    {"height", required_argument, NULL, 'h'},

    // RealESRGAN options
    {"gpuid", required_argument, NULL, 'g'},
    {"model", required_argument, NULL, 'm'},
    {"scale", required_argument, NULL, 'r'},
    {0, 0, 0, 0}
};

// Structure to hold parsed arguments
struct arguments {
    // General options
    const char *input_filename;
    const char *output_filename;
    const char *filter_type;

    // Encoder options
    const char *codec;
    const char *pix_fmt;
    const char *preset;
    int64_t bitrate;
    float crf;

    // libplacebo options
    const char *shader_path;
    int output_width;
    int output_height;

    // RealESRGAN options
    int gpuid;
    const char *model;
    int scaling_factor;
};

const char *valid_models[] = {
    "realesrgan-plus",
    "realesrgan-plus-anime",
    "realesr-animevideov3",
};

int is_valid_realesrgan_model(const char *model) {
    if (!model) {
        return 0;
    }
    for (int i = 0; i < sizeof(valid_models) / sizeof(valid_models[0]); i++) {
        if (strcmp(model, valid_models[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

void print_help() {
    printf("Usage: video2x [OPTIONS]\n");
    printf("\nGeneral Options:\n");
    printf("  -i, --input		Input video file path\n");
    printf("  -o, --output		Output video file path\n");
    printf("  -f, --filter		Filter to use: 'libplacebo' or 'realesrgan'\n");
    printf("  -v, --version		Print program version\n");
    printf("  --help		Display this help page\n");

    printf("\nEncoder Options (Optional):\n");
    printf("  -c, --codec		Output codec (default: libx264)\n");
    printf("  -p, --preset		Encoder preset (default: veryslow)\n");
    printf("  -x, --pixfmt		Output pixel format (default: yuv420p)\n");
    printf("  -b, --bitrate		Bitrate in bits per second (default: 2000000)\n");
    printf("  -q, --crf		Constant Rate Factor (default: 17.0)\n");

    printf("\nlibplacebo Options:\n");
    printf("  -s, --shader		Name or path to custom GLSL shader file\n");
    printf("  -w, --width		Output width\n");
    printf("  -h, --height		Output height\n");

    printf("\nRealESRGAN Options:\n");
    printf("  -g, --gpuid		Vulkan GPU ID (default: 0)\n");
    printf("  -m, --model		Name of the model to use\n");
    printf("  -r, --scale		Scaling factor (2, 3, or 4)\n");
}

void parse_arguments(int argc, char **argv, struct arguments *arguments) {
    int option_index = 0;
    int c;

    // Default argument values
    arguments->input_filename = NULL;
    arguments->output_filename = NULL;
    arguments->filter_type = NULL;

    // Encoder options
    arguments->codec = "libx264";
    arguments->preset = "veryslow";
    arguments->pix_fmt = "yuv420p";
    arguments->bitrate = 2 * 1000 * 1000;
    arguments->crf = 17.0;

    // libplacebo options
    arguments->shader_path = NULL;
    arguments->output_width = 0;
    arguments->output_height = 0;

    // RealESRGAN options
    arguments->gpuid = 0;
    arguments->model = NULL;
    arguments->scaling_factor = 0;

    while ((c = getopt_long(argc, argv, "i:o:f:c:x:p:b:q:s:w:h:r:m:v", long_options, &option_index)
           ) != -1) {
        switch (c) {
            case 'i':
                arguments->input_filename = optarg;
                break;
            case 'o':
                arguments->output_filename = optarg;
                break;
            case 'f':
                arguments->filter_type = optarg;
                break;
            case 'c':
                arguments->codec = optarg;
                break;
            case 'x':
                arguments->pix_fmt = optarg;
                break;
            case 'p':
                arguments->preset = optarg;
                break;
            case 'b':
                arguments->bitrate = strtoll(optarg, NULL, 10);
                if (arguments->bitrate <= 0) {
                    fprintf(stderr, "Error: Invalid bitrate specified.\n");
                    exit(1);
                }
                break;
            case 'q':
                arguments->crf = atof(optarg);
                if (arguments->crf < 0.0 || arguments->crf > 51.0) {
                    fprintf(stderr, "Error: CRF must be between 0 and 51.\n");
                    exit(1);
                }
                break;
            case 's':
                arguments->shader_path = optarg;
                break;
            case 'w':
                arguments->output_width = atoi(optarg);
                if (arguments->output_width <= 0) {
                    fprintf(stderr, "Error: Output width must be greater than 0.\n");
                    exit(1);
                }
                break;
            case 'h':
                arguments->output_height = atoi(optarg);
                if (arguments->output_height <= 0) {
                    fprintf(stderr, "Error: Output height must be greater than 0.\n");
                    exit(1);
                }
                break;
            case 'g':
                arguments->gpuid = atoi(optarg);
                break;
            case 'm':
                arguments->model = optarg;
                if (!is_valid_realesrgan_model(arguments->model)) {
                    fprintf(
                        stderr,
                        "Error: Invalid model specified. Must be 'realesrgan-plus', 'realesrgan-plus-anime', or 'realesr-animevideov3'.\n"
                    );
                    exit(1);
                }
                break;
            case 'r':
                arguments->scaling_factor = atoi(optarg);
                if (arguments->scaling_factor != 2 && arguments->scaling_factor != 3 &&
                    arguments->scaling_factor != 4) {
                    fprintf(stderr, "Error: Scaling factor must be 2, 3, or 4.\n");
                    exit(1);
                }
                break;
            case 'v':
                printf("video2x %s\n", VIDEO2X_VERSION);
                exit(0);
            case 0:  // Long-only options without short equivalents (e.g., help)
                if (strcmp(long_options[option_index].name, "help") == 0) {
                    print_help();
                    exit(0);
                }
                break;
            default:
                fprintf(stderr, "Invalid options provided.\n");
                exit(1);
        }
    }

    // Check for required arguments
    if (!arguments->input_filename || !arguments->output_filename) {
        fprintf(stderr, "Error: Input and output files are required.\n");
        exit(1);
    }

    if (!arguments->filter_type) {
        fprintf(stderr, "Error: Filter type is required (libplacebo or realesrgan).\n");
        exit(1);
    }

    if (strcmp(arguments->filter_type, "libplacebo") == 0) {
        if (!arguments->shader_path || arguments->output_width == 0 ||
            arguments->output_height == 0) {
            fprintf(
                stderr,
                "Error: For libplacebo, shader name/path (-s), width (-w), and height (-e) are required.\n"
            );
            exit(1);
        }
    } else if (strcmp(arguments->filter_type, "realesrgan") == 0) {
        if (arguments->scaling_factor == 0 || !arguments->model) {
            fprintf(
                stderr, "Error: For realesrgan, scaling factor (-r) and model (-m) are required.\n"
            );
            exit(1);
        }
    }
}

int main(int argc, char **argv) {
    struct arguments arguments;
    parse_arguments(argc, argv, &arguments);

    // Setup filter configurations based on the parsed arguments
    struct FilterConfig filter_config;
    if (strcmp(arguments.filter_type, "libplacebo") == 0) {
        filter_config.filter_type = FILTER_LIBPLACEBO;
        filter_config.config.libplacebo.output_width = arguments.output_width;
        filter_config.config.libplacebo.output_height = arguments.output_height;
        filter_config.config.libplacebo.shader_path = arguments.shader_path;
    } else if (strcmp(arguments.filter_type, "realesrgan") == 0) {
        filter_config.filter_type = FILTER_REALESRGAN;
        filter_config.config.realesrgan.gpuid = arguments.gpuid;
        filter_config.config.realesrgan.tta_mode = 0;
        filter_config.config.realesrgan.scaling_factor = arguments.scaling_factor;
        filter_config.config.realesrgan.model = arguments.model;
    } else {
        fprintf(stderr, "Error: Invalid filter type specified.\n");
        return 1;
    }

    // Parse codec to AVCodec
    const AVCodec *codec = avcodec_find_encoder_by_name(arguments.codec);
    if (!codec) {
        fprintf(stderr, "Error: Codec '%s' not found.\n", arguments.codec);
        return 1;
    }

    // Parse pixel format to AVPixelFormat
    enum AVPixelFormat pix_fmt = av_get_pix_fmt(arguments.pix_fmt);
    if (pix_fmt == AV_PIX_FMT_NONE) {
        fprintf(stderr, "Error: Invalid pixel format '%s'.\n", arguments.pix_fmt);
        return 1;
    }

    // Setup encoder configuration
    struct EncoderConfig encoder_config = {
        .output_width = 0,   // To be filled by libvideo2x
        .output_height = 0,  // To be filled by libvideo2x
        .codec = codec->id,
        .pix_fmt = pix_fmt,
        .preset = arguments.preset,
        .bit_rate = arguments.bitrate,
        .crf = arguments.crf,
    };

    // Setup struct to store processing status
    struct ProcessingStatus status = {0};

    // Process the video
    if (process_video(
            arguments.input_filename,
            arguments.output_filename,
            &filter_config,
            &encoder_config,
            &status
        )) {
        fprintf(stderr, "Video processing failed.\n");
        return 1;
    }

    // Print processing summary
    printf("====== Video2X Processing summary ======\n");
    printf("Video file processed: %s\n", arguments.input_filename);
    printf("Total frames processed: %ld\n", status.processed_frames);
    printf("Total time taken: %lds\n", time(NULL) - status.start_time);
    printf("Output written to: %s\n", arguments.output_filename);
    return 0;
}
