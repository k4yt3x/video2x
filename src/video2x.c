#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <time.h>

#ifdef _WIN32
#include <conio.h>
#else
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>

#include <libvideo2x.h>

#include "getopt.h"

const char *VIDEO2X_VERSION = "6.0.0";

// Set UNIX terminal input to non-blocking mode
#ifndef _WIN32
void set_nonblocking_input(bool enable) {
    static struct termios oldt, newt;
    if (enable) {
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    } else {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        fcntl(STDIN_FILENO, F_SETFL, 0);
    }
}
#endif

// Define command line options
static struct option long_options[] = {
    {"version", no_argument, NULL, 'v'},
    {"help", no_argument, NULL, 0},

    // General options
    {"input", required_argument, NULL, 'i'},
    {"output", required_argument, NULL, 'o'},
    {"filter", required_argument, NULL, 'f'},
    {"hwaccel", required_argument, NULL, 'a'},
    {"nocopystreams", no_argument, NULL, 0},
    {"benchmark", no_argument, NULL, 0},

    // Encoder options
    {"codec", required_argument, NULL, 'c'},
    {"preset", required_argument, NULL, 'p'},
    {"pixfmt", required_argument, NULL, 'x'},
    {"bitrate", required_argument, NULL, 'b'},
    {"crf", required_argument, NULL, 'q'},

    // libplacebo options
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
    const char *hwaccel;
    bool nocopystreams;
    bool benchmark;

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

struct ProcessVideoThreadArguments {
    struct arguments *arguments;
    enum AVHWDeviceType hw_device_type;
    struct FilterConfig *filter_config;
    struct EncoderConfig *encoder_config;
    struct VideoProcessingContext *proc_ctx;
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
    printf("\nOptions:\n");
    printf("  -v, --version		Print program version\n");
    printf("  -?, --help		Display this help page\n");
    printf("\nGeneral Processing Options:\n");
    printf("  -i, --input		Input video file path\n");
    printf("  -o, --output		Output video file path\n");
    printf("  -f, --filter		Filter to use: 'libplacebo' or 'realesrgan'\n");
    printf("  -a, --hwaccel		Hardware acceleration method (default: none)\n");
    printf("  --nocopystreams	Do not copy audio and subtitle streams\n");
    printf("  --benchmark		Discard processed frames and calculate average FPS\n");

    printf("\nEncoder Options (Optional):\n");
    printf("  -c, --codec		Output codec (default: libx264)\n");
    printf("  -p, --preset		Encoder preset (default: slow)\n");
    printf("  -x, --pixfmt		Output pixel format (default: auto)\n");
    printf("  -b, --bitrate		Bitrate in bits per second (default: 0 (VBR))\n");
    printf("  -q, --crf		Constant Rate Factor (default: 20.0)\n");

    printf("\nlibplacebo Options:\n");
    printf("  -s, --shader		Name or path to custom GLSL shader file\n");
    printf("  -w, --width		Output width\n");
    printf("  -h, --height		Output height\n");

    printf("\nRealESRGAN Options:\n");
    printf("  -g, --gpuid		Vulkan GPU ID (default: 0)\n");
    printf("  -m, --model		Name of the model to use\n");
    printf("  -r, --scale		Scaling factor (2, 3, or 4)\n");

    printf("\nExamples Usage:\n");
    printf("  video2x -i in.mp4 -o out.mp4 -f libplacebo -s anime4k-mode-a -w 3840 -h 2160\n");
    printf("  video2x -i in.mp4 -o out.mp4 -f realesrgan -m realesr-animevideov3 -r 4\n");
}

void parse_arguments(int argc, char **argv, struct arguments *arguments) {
    int option_index = 0;
    int c;

    // Default argument values
    arguments->input_filename = NULL;
    arguments->output_filename = NULL;
    arguments->filter_type = NULL;
    arguments->hwaccel = "none";
    arguments->nocopystreams = false;
    arguments->benchmark = false;

    // Encoder options
    arguments->codec = "libx264";
    arguments->preset = "slow";
    arguments->pix_fmt = NULL;
    arguments->bitrate = 0;
    arguments->crf = 20.0;

    // libplacebo options
    arguments->shader_path = NULL;
    arguments->output_width = 0;
    arguments->output_height = 0;

    // RealESRGAN options
    arguments->gpuid = 0;
    arguments->model = NULL;
    arguments->scaling_factor = 0;

    while ((c = getopt_long(
                argc, argv, "i:o:f:a:c:x:p:b:q:s:w:h:r:m:v", long_options, &option_index
            )) != -1) {
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
            case 'a':
                arguments->hwaccel = optarg;
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
                        "Error: Invalid model specified. Must be 'realesrgan-plus', "
                        "'realesrgan-plus-anime', or 'realesr-animevideov3'.\n"
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
                printf("Video2X v%s\n", VIDEO2X_VERSION);
                exit(0);
            case 0:  // Long-only options without short equivalents
                if (strcmp(long_options[option_index].name, "help") == 0) {
                    print_help();
                    exit(0);
                } else if (strcmp(long_options[option_index].name, "nocopystreams") == 0) {
                    arguments->nocopystreams = true;
                } else if (strcmp(long_options[option_index].name, "benchmark") == 0) {
                    arguments->benchmark = true;
                }
                break;
            default:
                fprintf(stderr, "Invalid options provided.\n");
                exit(1);
        }
    }

    // Check for required arguments
    if (!arguments->input_filename) {
        fprintf(stderr, "Error: Input file path is required.\n");
        exit(1);
    }

    if (!arguments->output_filename && !arguments->benchmark) {
        fprintf(stderr, "Error: Output file path is required.\n");
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
                "Error: For libplacebo, shader name/path (-s), width (-w), "
                "and height (-e) are required.\n"
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

// Wrapper function for video processing thread
int process_video_thread(void *arg) {
    struct ProcessVideoThreadArguments *thread_args = (struct ProcessVideoThreadArguments *)arg;

    // Extract individual arguments
    struct arguments *arguments = thread_args->arguments;
    enum AVHWDeviceType hw_device_type = thread_args->hw_device_type;
    struct FilterConfig *filter_config = thread_args->filter_config;
    struct EncoderConfig *encoder_config = thread_args->encoder_config;
    struct VideoProcessingContext *proc_ctx = thread_args->proc_ctx;

    // Call the process_video function
    int result = process_video(
        arguments->input_filename,
        arguments->output_filename,
        arguments->benchmark,
        hw_device_type,
        filter_config,
        encoder_config,
        proc_ctx
    );

    proc_ctx->completed = true;
    return result;
}

int main(int argc, char **argv) {
    // Print help if no arguments are provided
    if (argc < 2) {
        print_help();
        return 1;
    }

    // Parse command line arguments
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
    enum AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;
    if (arguments.pix_fmt) {
        pix_fmt = av_get_pix_fmt(arguments.pix_fmt);
        if (pix_fmt == AV_PIX_FMT_NONE) {
            fprintf(stderr, "Error: Invalid pixel format '%s'.\n", arguments.pix_fmt);
            return 1;
        }
    }

    // Setup encoder configuration
    struct EncoderConfig encoder_config = {
        .output_width = 0,   // To be filled by libvideo2x
        .output_height = 0,  // To be filled by libvideo2x
        .copy_streams = !arguments.nocopystreams,
        .codec = codec->id,
        .pix_fmt = pix_fmt,
        .preset = arguments.preset,
        .bit_rate = arguments.bitrate,
        .crf = arguments.crf,
    };

    // Parse hardware acceleration method
    enum AVHWDeviceType hw_device_type = AV_HWDEVICE_TYPE_NONE;
    if (strcmp(arguments.hwaccel, "none") != 0) {
        hw_device_type = av_hwdevice_find_type_by_name(arguments.hwaccel);
        if (hw_device_type == AV_HWDEVICE_TYPE_NONE) {
            fprintf(stderr, "Error: Invalid hardware device type '%s'.\n", arguments.hwaccel);
            return 1;
        }
    }

    // Setup struct to store processing context
    struct VideoProcessingContext proc_ctx = {
        .processed_frames = 0,
        .total_frames = 0,
        .start_time = time(NULL),
        .pause = false,
        .abort = false,
        .completed = false
    };

    // Create a ThreadArguments struct to hold all the arguments for the thread
    struct ProcessVideoThreadArguments thread_args = {
        .arguments = &arguments,
        .hw_device_type = hw_device_type,
        .filter_config = &filter_config,
        .encoder_config = &encoder_config,
        .proc_ctx = &proc_ctx
    };

// Enable non-blocking input
#ifndef _WIN32
    set_nonblocking_input(true);
#endif

    // Create a thread for video processing
    thrd_t processing_thread;
    if (thrd_create(&processing_thread, process_video_thread, &thread_args) != thrd_success) {
        fprintf(stderr, "Failed to create processing thread\n");
        return 1;
    }
    printf("[Video2X] Video processing started.\n");
    printf("[Video2X] Press SPACE to pause/resume, 'q' to abort.\n");

    // Main thread loop to display progress and handle input
    while (!proc_ctx.completed) {
        // Check for key presses
        int ch = -1;

        // Check for key press
#ifdef _WIN32
        if (_kbhit()) {
            ch = _getch();
        }
#else
        ch = getchar();
#endif

        if (ch == ' ' || ch == '\n') {
            // Toggle pause state
            proc_ctx.pause = !proc_ctx.pause;
            if (proc_ctx.pause) {
                printf("\n[Video2X] Processing paused. Press SPACE to resume, 'q' to abort.");
            } else {
                printf("\n[Video2X] Resuming processing...");
            }
            fflush(stdout);
        } else if (ch == 'q' || ch == 'Q') {
            // Abort processing
            printf("\n[Video2X] Aborting processing...");
            fflush(stdout);
            proc_ctx.abort = true;
            break;
        }

        // Display progress
        if (!proc_ctx.pause && proc_ctx.total_frames > 0) {
            printf(
                "\r[Video2X] Processing frame %ld/%ld (%.2f%%); time elapsed: %lds",
                proc_ctx.processed_frames,
                proc_ctx.total_frames,
                proc_ctx.total_frames > 0
                    ? proc_ctx.processed_frames * 100.0 / proc_ctx.total_frames
                    : 0.0,
                time(NULL) - proc_ctx.start_time
            );
            fflush(stdout);
        }

        // Sleep for a short duration
        thrd_sleep(&(struct timespec){.tv_sec = 0, .tv_nsec = 100000000}, NULL);  // Sleep for 100ms
    }
    puts("");  // Print newline after progress bar is complete

// Restore terminal to blocking mode
#ifndef _WIN32
    set_nonblocking_input(false);
#endif

    // Join the processing thread to ensure it completes before exiting
    int process_result;
    thrd_join(processing_thread, &process_result);

    if (proc_ctx.abort) {
        fprintf(stderr, "Video processing aborted\n");
        return 2;
    }

    if (process_result != 0) {
        fprintf(stderr, "Video processing failed\n");
        return process_result;
    }

    // Calculate statistics
    time_t time_elapsed = time(NULL) - proc_ctx.start_time;
    float average_speed_fps =
        (float)proc_ctx.processed_frames / (time_elapsed > 0 ? time_elapsed : 1);

    // Print processing summary
    printf("====== Video2X %s summary ======\n", arguments.benchmark ? "Benchmark" : "Processing");
    printf("Video file processed: %s\n", arguments.input_filename);
    printf("Total frames processed: %ld\n", proc_ctx.processed_frames);
    printf("Total time taken: %lds\n", time_elapsed);
    printf("Average processing speed: %.2f FPS\n", average_speed_fps);

    // Print additional information if not in benchmark mode
    if (!arguments.benchmark) {
        printf("Output written to: %s\n", arguments.output_filename);
    }

    return 0;
}
