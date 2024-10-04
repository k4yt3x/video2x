#include <argp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libvideo2x.h>

const char *VIDEO2X_VERSION = "6.0.0";

// Program documentation
static char doc[] = "";
static char args_doc[] = "";

// Define the options struct
static struct argp_option options[] = {
    {"input", 'i', "INPUT", 0, "Input video file (required)"},
    {"output", 'o', "OUTPUT", 0, "Output video file (required)"},
    {"filter", 'f', "FILTER", 0, "Filter to use: 'libplacebo' or 'realesrgan' (required)"},

    // libplacebo
    {"shader", 's', "SHADER", 0, "Path to custom GLSL shader file (libplacebo only)"},
    {"width", 'w', "WIDTH", 0, "Output width (libplacebo only)"},
    {"height", 'h', "HEIGHT", 0, "Output height (libplacebo only)"},

    // realesrgan
    {"scale", 'r', "SCALE", 0, "Scaling factor (2, 3, or 4 for realesrgan only)"},
    {"model", 'm', "MODEL", 0, "Name of the model to use (realesrgan only)"},

    {"version", 'v', 0, 0, "Print program version"},
    {0}  // Last element must be 0 to signal the end of options
};

// Structure to hold parsed arguments
struct arguments {
    const char *input_filename;
    const char *output_filename;
    const char *filter_type;
    const char *shader_path;
    const char *model;
    int output_width;
    int output_height;
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

// Parse a single option
static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    struct arguments *arguments = state->input;

    switch (key) {
        case 'i':
            arguments->input_filename = arg;
            break;
        case 'o':
            arguments->output_filename = arg;
            break;
        case 'f':
            arguments->filter_type = arg;
            break;
        case 's':
            arguments->shader_path = arg;
            break;
        case 'w':
            arguments->output_width = atoi(arg);
            break;
        case 'h':
            arguments->output_height = atoi(arg);
            break;
        case 'r':
            arguments->scaling_factor = atoi(arg);
            if (arguments->scaling_factor != 2 && arguments->scaling_factor != 3 &&
                arguments->scaling_factor != 4) {
                argp_error(state, "Invalid scaling factor: Must be 2, 3, or 4.");
            }
            break;
        case 'm':
            arguments->model = arg;
            break;
        case 'v':
            printf("video2x %s\n", VIDEO2X_VERSION);
            exit(0);
        case ARGP_KEY_ARG:
            if (state->arg_num > 0) {
                argp_usage(state);
            }
            break;
        case ARGP_KEY_END:
            // Ensure mandatory arguments are provided
            if (!arguments->input_filename || !arguments->output_filename) {
                argp_failure(state, 1, 0, "Input and output files are required.");
            }

            // Ensure filter is specified
            if (!arguments->filter_type) {
                argp_failure(state, 1, 0, "Filter type (libplacebo or realesrgan) is required.");
            }

            // Validate filter and related options
            if (!arguments->filter_type) {
                argp_failure(state, 1, 0, "Filter type is required.");
            }
            if (strcmp(arguments->filter_type, "libplacebo") == 0) {
                // Libplacebo specific validations
                if (!arguments->output_width && !arguments->output_height) {
                    argp_failure(
                        state, 1, 0, "Either width or height must be specified for libplacebo."
                    );
                }
                if (!arguments->shader_path) {
                    argp_failure(state, 1, 0, "Shader path is required for libplacebo.");
                }
                if (arguments->scaling_factor) {
                    argp_failure(state, 1, 0, "Scaling factor is not valid for libplacebo.");
                }
            } else if (strcmp(arguments->filter_type, "realesrgan") == 0) {
                // RealESRGAN specific validations
                if (!arguments->scaling_factor) {
                    argp_failure(state, 1, 0, "Scaling factor is required for realesrgan.");
                }
                if (!arguments->model) {
                    argp_failure(
                        state,
                        1,
                        0,
                        "Model name is required for realesrgan. Must be 'realesrgan-plus', 'realesrgan-plus-anime', or 'realesr-animevideov3'."
                    );
                }
                if (!is_valid_realesrgan_model(arguments->model)) {
                    argp_failure(
                        state,
                        1,
                        0,
                        "Invalid model specified. Must be 'realesrgan-plus', 'realesrgan-plus-anime', or 'realesr-animevideov3'."
                    );
                }
            } else {
                argp_failure(
                    state,
                    1,
                    0,
                    "Invalid filter type specified. Must be 'libplacebo' or 'realesrgan'."
                );
            }

            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

// Our argp parser
static struct argp argp = {options, parse_opt, args_doc, doc};

int main(int argc, char **argv) {
    struct arguments arguments;

    // Default argument values
    arguments.input_filename = NULL;
    arguments.output_filename = NULL;
    arguments.filter_type = NULL;
    arguments.shader_path = NULL;
    arguments.model = NULL;
    arguments.output_width = 0;
    arguments.output_height = 0;
    arguments.scaling_factor = 0;

    // Parse command-line arguments
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    // Setup filter configurations based on the parsed arguments
    struct FilterConfig filter_config;
    if (strcmp(arguments.filter_type, "libplacebo") == 0) {
        filter_config.filter_type = FILTER_LIBPLACEBO;
        filter_config.config.libplacebo.output_width = arguments.output_width;
        filter_config.config.libplacebo.output_height = arguments.output_height;
        filter_config.config.libplacebo.shader_path = arguments.shader_path;
    } else if (strcmp(arguments.filter_type, "realesrgan") == 0) {
        filter_config.filter_type = FILTER_REALESRGAN;
        filter_config.config.realesrgan.gpuid = 0;
        filter_config.config.realesrgan.tta_mode = 0;
        filter_config.config.realesrgan.scaling_factor = arguments.scaling_factor;
        filter_config.config.realesrgan.model = arguments.model;
    } else {
        fprintf(stderr, "Error: Invalid filter type specified.\n");
        return 1;
    }

    // Setup encoder configuration
    struct EncoderConfig encoder_config = {
        .output_width = 0,   // To be filled by libvideo2x
        .output_height = 0,  // To be filled by libvideo2x
        .codec = AV_CODEC_ID_H264,
        .pix_fmt = AV_PIX_FMT_YUV420P,
        .bit_rate = 2 * 1000 * 1000,  // 2 Mbps
        .crf = 17.0,
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
