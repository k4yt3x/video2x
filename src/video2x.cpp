#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>

#ifdef _WIN32
#include <Windows.h>
#include <conio.h>
#else
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>

#include <libvideo2x/libvideo2x.h>
#include <libvideo2x/version.h>
}

#include <spdlog/spdlog.h>

#ifdef _WIN32
#define BOOST_PROGRAM_OPTIONS_WCHAR_T
#endif
#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include "libvideo2x/timer.h"

// Indicate if a newline needs to be printed before the next output
std::atomic<bool> newline_required = false;

// Structure to hold parsed arguments
struct Arguments {
    // General options
#ifdef _WIN32
    std::wstring loglevel = L"info";
    std::wstring filter_type;
    std::wstring hwaccel = L"none";
#else
    std::string loglevel = "info";
    std::string filter_type;
    std::string hwaccel = "none";
#endif
    bool noprogress = false;
    std::filesystem::path in_fname;
    std::filesystem::path out_fname;
    bool nocopystreams = false;
    bool benchmark = false;

    // Encoder options
#ifdef _WIN32
    std::wstring codec = L"libx264";
    std::wstring pix_fmt;
    std::wstring preset = L"slow";
#else
    std::string codec = "libx264";
    std::string pix_fmt;
    std::string preset = "slow";
#endif
    int64_t bitrate = 0;
    float crf = 20.0f;

    // libplacebo options
    std::filesystem::path shader_path;
    int out_width = 0;
    int out_height = 0;

    // RealESRGAN options
#ifdef _WIN32
    std::wstring model_name;
#else
    std::string model_name;
#endif
    int gpuid = 0;
    int scaling_factor = 0;
};

// Set UNIX terminal input to non-blocking mode
#ifndef _WIN32
void set_nonblocking_input(bool enable) {
    static struct termios oldt, newt;
    if (enable) {
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= static_cast<unsigned int>(~(ICANON | ECHO));
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    } else {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        fcntl(STDIN_FILENO, F_SETFL, 0);
    }
}
#endif

// Convert a wide string to UTF-8 string
#ifdef _WIN32
std::string wstring_to_utf8(const std::wstring &wstr) {
    if (wstr.empty()) {
        return std::string();
    }
    int size_needed = WideCharToMultiByte(
        CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr
    );
    std::string converted_str(size_needed, 0);
    WideCharToMultiByte(
        CP_UTF8,
        0,
        wstr.data(),
        static_cast<int>(wstr.size()),
        &converted_str[0],
        size_needed,
        nullptr,
        nullptr
    );
    return converted_str;
}
#else
std::string wstring_to_utf8(const std::string &str) {
    return str;
}
#endif

// Newline-safe log callback for FFmpeg
void newline_safe_ffmpeg_log_callback(void *ptr, int level, const char *fmt, va_list vl) {
    if (level <= av_log_get_level() && newline_required) {
        putchar('\n');
        newline_required = false;
    }
    av_log_default_callback(ptr, level, fmt, vl);
}

bool is_valid_realesrgan_model(const std::string &model) {
    static const std::unordered_set<std::string> valid_realesrgan_models = {
        "realesrgan-plus", "realesrgan-plus-anime", "realesr-animevideov3"
    };
    return valid_realesrgan_models.count(model) > 0;
}

enum Libvideo2xLogLevel parse_log_level(const std::string &level_name) {
    if (level_name == "trace") {
        return LIBVIDEO2X_LOG_LEVEL_TRACE;
    } else if (level_name == "debug") {
        return LIBVIDEO2X_LOG_LEVEL_DEBUG;
    } else if (level_name == "info") {
        return LIBVIDEO2X_LOG_LEVEL_INFO;
    } else if (level_name == "warning" || level_name == "warn") {
        return LIBVIDEO2X_LOG_LEVEL_WARNING;
    } else if (level_name == "error") {
        return LIBVIDEO2X_LOG_LEVEL_ERROR;
    } else if (level_name == "critical") {
        return LIBVIDEO2X_LOG_LEVEL_CRITICAL;
    } else if (level_name == "off" || level_name == "none") {
        return LIBVIDEO2X_LOG_LEVEL_OFF;
    } else {
        spdlog::warn("Invalid log level specified. Defaulting to 'info'.");
        return LIBVIDEO2X_LOG_LEVEL_INFO;
    }
}

// Mutex for synchronizing access to VideoProcessingContext
std::mutex proc_ctx_mutex;

// Wrapper function for video processing thread
void process_video_thread(
    Arguments *arguments,
    int *proc_ret,
    AVHWDeviceType hw_device_type,
    FilterConfig *filter_config,
    EncoderConfig *encoder_config,
    VideoProcessingContext *proc_ctx
) {
    enum Libvideo2xLogLevel log_level = parse_log_level(wstring_to_utf8(arguments->loglevel));

#ifdef _WIN32
    const wchar_t *in_fname = arguments->in_fname.c_str();
    const wchar_t *out_fname = arguments->out_fname.c_str();
#else
    const char *in_fname = arguments->in_fname.c_str();
    const char *out_fname = arguments->out_fname.c_str();
#endif

    *proc_ret = process_video(
        in_fname,
        out_fname,
        log_level,
        arguments->benchmark,
        hw_device_type,
        filter_config,
        encoder_config,
        proc_ctx
    );

    {
        std::lock_guard<std::mutex> lock(proc_ctx_mutex);
        proc_ctx->completed = true;
    }
}

#ifdef _WIN32
int wmain(int argc, wchar_t *argv[]) {
    SetConsoleOutputCP(CP_UTF8);
#else
int main(int argc, char **argv) {
#endif
    // Initialize arguments structure
    Arguments arguments;

    // Parse command line arguments using Boost.Program_options
    try {
        po::options_description desc("Allowed options");

#ifdef _WIN32
        desc.add_options()
            ("help", "Display this help page")
            ("version,v", "Print program version")
            ("loglevel", po::wvalue<std::wstring>(&arguments.loglevel), "Set log level (trace, debug, info, warn, error, critical, none)")
            ("noprogress", po::bool_switch(&arguments.noprogress), "Do not display the progress bar")

            // General Processing Options
            ("input,i", po::wvalue<std::wstring>(), "Input video file path")
            ("output,o", po::wvalue<std::wstring>(), "Output video file path")
            ("filter,f", po::wvalue<std::wstring>(&arguments.filter_type), "Filter to use: 'libplacebo' or 'realesrgan'")
            ("hwaccel,a", po::wvalue<std::wstring>(&arguments.hwaccel), "Hardware acceleration method (default: none)")
            ("nocopystreams", po::bool_switch(&arguments.nocopystreams), "Do not copy audio and subtitle streams")
            ("benchmark", po::bool_switch(&arguments.benchmark), "Discard processed frames and calculate average FPS")

            // Encoder options
            ("codec,c", po::wvalue<std::wstring>(&arguments.codec), "Output codec (default: libx264)")
            ("preset,p", po::wvalue<std::wstring>(&arguments.preset), "Encoder preset (default: slow)")
            ("pixfmt,x", po::wvalue<std::wstring>(&arguments.pix_fmt), "Output pixel format (default: auto)")
            ("bitrate,b", po::value<int64_t>(&arguments.bitrate)->default_value(0), "Bitrate in bits per second (default: 0 (VBR))")
            ("crf,q", po::value<float>(&arguments.crf)->default_value(20.0f), "Constant Rate Factor (default: 20.0)")

            // libplacebo options
            ("shader,s", po::wvalue<std::wstring>(), "Name or path of the GLSL shader file to use")
            ("width,w", po::value<int>(&arguments.out_width), "Output width")
            ("height,h", po::value<int>(&arguments.out_height), "Output height")

            // RealESRGAN options
            ("gpuid,g", po::value<int>(&arguments.gpuid)->default_value(0), "Vulkan GPU ID (default: 0)")
            ("model,m", po::wvalue<std::wstring>(&arguments.model_name), "Name of the model to use")
            ("scale,r", po::value<int>(&arguments.scaling_factor), "Scaling factor (2, 3, or 4)")
        ;
#else
        desc.add_options()
            ("help", "Display this help page")
            ("version,v", "Print program version")
            ("loglevel", po::value<std::string>(&arguments.loglevel)->default_value("info"), "Set log level (trace, debug, info, warn, error, critical, none)")
            ("noprogress", po::bool_switch(&arguments.noprogress), "Do not display the progress bar")

            // General Processing Options
            ("input,i", po::value<std::string>(), "Input video file path")
            ("output,o", po::value<std::string>(), "Output video file path")
            ("filter,f", po::value<std::string>(&arguments.filter_type), "Filter to use: 'libplacebo' or 'realesrgan'")
            ("hwaccel,a", po::value<std::string>(&arguments.hwaccel)->default_value("none"), "Hardware acceleration method (default: none)")
            ("nocopystreams", po::bool_switch(&arguments.nocopystreams), "Do not copy audio and subtitle streams")
            ("benchmark", po::bool_switch(&arguments.benchmark), "Discard processed frames and calculate average FPS")

            // Encoder options
            ("codec,c", po::value<std::string>(&arguments.codec)->default_value("libx264"), "Output codec (default: libx264)")
            ("preset,p", po::value<std::string>(&arguments.preset)->default_value("slow"), "Encoder preset (default: slow)")
            ("pixfmt,x", po::value<std::string>(&arguments.pix_fmt), "Output pixel format (default: auto)")
            ("bitrate,b", po::value<int64_t>(&arguments.bitrate)->default_value(0), "Bitrate in bits per second (default: 0 (VBR))")
            ("crf,q", po::value<float>(&arguments.crf)->default_value(20.0f), "Constant Rate Factor (default: 20.0)")

            // libplacebo options
            ("shader,s", po::value<std::string>(), "Name or path of the GLSL shader file to use (built-in: 'anime4k-a', 'anime4k-b', 'anime4k-c', 'anime4k-a+a', 'anime4k-b+b', 'anime4k-c+a')")
            ("width,w", po::value<int>(&arguments.out_width), "Output width")
            ("height,h", po::value<int>(&arguments.out_height), "Output height")

            // RealESRGAN options
            ("gpuid,g", po::value<int>(&arguments.gpuid)->default_value(0), "Vulkan GPU ID (default: 0)")
            ("model,m", po::value<std::string>(&arguments.model_name), "Name of the model to use")
            ("scale,r", po::value<int>(&arguments.scaling_factor), "Scaling factor (2, 3, or 4)")
        ;
#endif

        // Positional arguments
        po::positional_options_description p;
        p.add("input", 1).add("output", 1).add("filter", 1);

#ifdef _WIN32
        po::variables_map vm;
        po::store(po::wcommand_line_parser(argc, argv).options(desc).positional(p).run(), vm);
#else
        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
#endif
        po::notify(vm);

        // Set default values for optional arguments
#ifdef _WIN32
        if (!vm.count("loglevel")) {
            arguments.loglevel = L"info";
        }
        if (!vm.count("hwaccel")) {
            arguments.hwaccel = L"none";
        }
        if (!vm.count("codec")) {
            arguments.codec = L"libx264";
        }
        if (!vm.count("preset")) {
            arguments.preset = L"slow";
        }
#endif

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        }

        if (vm.count("version")) {
            std::cout << "Video2X version " << LIBVIDEO2X_VERSION_STRING << std::endl;
            return 0;
        }

        // Assign positional arguments
        if (vm.count("input")) {
#ifdef _WIN32
            arguments.in_fname = std::filesystem::path(vm["input"].as<std::wstring>());
#else
            arguments.in_fname = std::filesystem::path(vm["input"].as<std::string>());
#endif
        } else {
            spdlog::error("Error: Input file path is required.");
            return 1;
        }

        if (vm.count("output")) {
#ifdef _WIN32
            arguments.out_fname = std::filesystem::path(vm["output"].as<std::wstring>());
#else
            arguments.out_fname = std::filesystem::path(vm["output"].as<std::string>());
#endif
        } else if (!arguments.benchmark) {
            spdlog::error("Error: Output file path is required.");
            return 1;
        }

        if (!vm.count("filter")) {
            spdlog::error("Error: Filter type is required (libplacebo or realesrgan).");
            return 1;
        }

        if (vm.count("shader")) {
#ifdef _WIN32
            arguments.shader_path = std::filesystem::path(vm["shader"].as<std::wstring>());
#else
            arguments.shader_path = std::filesystem::path(vm["shader"].as<std::string>());
#endif
        }

        if (vm.count("model")) {
#ifdef _WIN32
            bool is_valid_model =
                is_valid_realesrgan_model(wstring_to_utf8(vm["model"].as<std::wstring>()));
#else
            bool is_valid_model = is_valid_realesrgan_model(vm["model"].as<std::string>());
#endif
            if (!is_valid_model) {
                spdlog::error(
                    "Error: Invalid model specified. Must be 'realesrgan-plus', "
                    "'realesrgan-plus-anime', or 'realesr-animevideov3'."
                );
                return 1;
            }
        }
    } catch (const po::error &e) {
        spdlog::error("Error parsing options: {}", e.what());
        return 1;
    } catch (const std::exception &e) {
        spdlog::error("Unexpected exception caught while parsing options: {}", e.what());
        return 1;
    }

// Additional validations
#ifdef _WIN32
    if (arguments.filter_type == L"libplacebo") {
#else
    if (arguments.filter_type == "libplacebo") {
#endif
        if (arguments.shader_path.empty() || arguments.out_width == 0 ||
            arguments.out_height == 0) {
            spdlog::error(
                "Error: For libplacebo, shader name/path (-s), width (-w), "
                "and height (-h) are required."
            );
            return 1;
        }
#ifdef _WIN32
    } else if (arguments.filter_type == L"realesrgan") {
#else
    } else if (arguments.filter_type == "realesrgan") {
#endif
        if (arguments.scaling_factor == 0 || arguments.model_name.empty()) {
            spdlog::error("Error: For realesrgan, scaling factor (-r) and model (-m) are required."
            );
            return 1;
        }
        if (arguments.scaling_factor != 2 && arguments.scaling_factor != 3 &&
            arguments.scaling_factor != 4) {
            spdlog::error("Error: Scaling factor must be 2, 3, or 4.");
            return 1;
        }
    } else {
        spdlog::error("Error: Invalid filter type specified. Must be 'libplacebo' or 'realesrgan'."
        );
        return 1;
    }

    // Validate bitrate
    if (arguments.bitrate < 0) {
        spdlog::error("Error: Invalid bitrate specified.");
        return 1;
    }

    // Validate CRF
    if (arguments.crf < 0.0f || arguments.crf > 51.0f) {
        spdlog::error("Error: CRF must be between 0 and 51.");
        return 1;
    }

    // Parse codec to AVCodec
    const AVCodec *codec = avcodec_find_encoder_by_name(wstring_to_utf8(arguments.codec).c_str());
    if (!codec) {
        spdlog::error("Error: Codec '{}' not found.", wstring_to_utf8(arguments.codec));
        return 1;
    }

    // Parse pixel format to AVPixelFormat
    enum AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;
    if (!arguments.pix_fmt.empty()) {
        pix_fmt = av_get_pix_fmt(wstring_to_utf8(arguments.pix_fmt).c_str());
        if (pix_fmt == AV_PIX_FMT_NONE) {
            spdlog::error("Error: Invalid pixel format '{}'.", wstring_to_utf8(arguments.pix_fmt));
            return 1;
        }
    }

    // Set spdlog log level
    auto log_level = parse_log_level(wstring_to_utf8(arguments.loglevel));
    switch (log_level) {
        case LIBVIDEO2X_LOG_LEVEL_TRACE:
            spdlog::set_level(spdlog::level::trace);
            break;
        case LIBVIDEO2X_LOG_LEVEL_DEBUG:
            spdlog::set_level(spdlog::level::debug);
            break;
        case LIBVIDEO2X_LOG_LEVEL_INFO:
            spdlog::set_level(spdlog::level::info);
            break;
        case LIBVIDEO2X_LOG_LEVEL_WARNING:
            spdlog::set_level(spdlog::level::warn);
            break;
        case LIBVIDEO2X_LOG_LEVEL_ERROR:
            spdlog::set_level(spdlog::level::err);
            break;
        case LIBVIDEO2X_LOG_LEVEL_CRITICAL:
            spdlog::set_level(spdlog::level::critical);
            break;
        case LIBVIDEO2X_LOG_LEVEL_OFF:
            spdlog::set_level(spdlog::level::off);
            break;
        default:
            spdlog::set_level(spdlog::level::info);
            break;
    }

    // Setup filter configurations based on the parsed arguments
    FilterConfig filter_config;
#ifdef _WIN32
    if (arguments.filter_type == L"libplacebo") {
#else
    if (arguments.filter_type == "libplacebo") {
#endif
        filter_config.filter_type = FILTER_LIBPLACEBO;
        filter_config.config.libplacebo.out_width = arguments.out_width;
        filter_config.config.libplacebo.out_height = arguments.out_height;
#ifdef _WIN32
        filter_config.config.libplacebo.shader_path = arguments.shader_path.c_str();
#else
        filter_config.config.libplacebo.shader_path = arguments.shader_path.c_str();
#endif
#ifdef _WIN32
    } else if (arguments.filter_type == L"realesrgan") {
#else
    } else if (arguments.filter_type == "realesrgan") {
#endif
        filter_config.filter_type = FILTER_REALESRGAN;
        filter_config.config.realesrgan.gpuid = arguments.gpuid;
        filter_config.config.realesrgan.tta_mode = false;
        filter_config.config.realesrgan.scaling_factor = arguments.scaling_factor;
#ifdef _WIN32
        filter_config.config.realesrgan.model_name = arguments.model_name.c_str();
#else
        filter_config.config.realesrgan.model_name = arguments.model_name.c_str();
#endif
    }

    // Convert arguments to UTF-8 encoded strings
    std::string preset_str = wstring_to_utf8(arguments.preset);

    // Setup encoder configuration
    EncoderConfig encoder_config;
    encoder_config.out_width = 0;
    encoder_config.out_height = 0;
    encoder_config.copy_streams = !arguments.nocopystreams;
    encoder_config.codec = codec->id;
    encoder_config.pix_fmt = pix_fmt;
    encoder_config.preset = preset_str.c_str();
    encoder_config.bit_rate = arguments.bitrate;
    encoder_config.crf = arguments.crf;

    // Parse hardware acceleration method
    enum AVHWDeviceType hw_device_type = AV_HWDEVICE_TYPE_NONE;
#ifdef _WIN32
    if (arguments.hwaccel != L"none") {
#else
    if (arguments.hwaccel != "none") {
#endif
        hw_device_type = av_hwdevice_find_type_by_name(wstring_to_utf8(arguments.hwaccel).c_str());
        if (hw_device_type == AV_HWDEVICE_TYPE_NONE) {
            spdlog::error(
                "Error: Invalid hardware device type '{}'.", wstring_to_utf8(arguments.hwaccel)
            );
            return 1;
        }
    }

    // Setup struct to store processing context
    VideoProcessingContext proc_ctx;
    proc_ctx.processed_frames = 0;
    proc_ctx.total_frames = 0;
    proc_ctx.pause = false;
    proc_ctx.abort = false;
    proc_ctx.completed = false;

    // Register a newline-safe log callback for FFmpeg
    av_log_set_callback(newline_safe_ffmpeg_log_callback);

    // Create a thread for video processing
    int proc_ret = 0;
    std::thread processing_thread(
        process_video_thread,
        &arguments,
        &proc_ret,
        hw_device_type,
        &filter_config,
        &encoder_config,
        &proc_ctx
    );
    spdlog::info("Press SPACE to pause/resume, 'q' to abort.");

    // Setup timer
    Timer timer;
    timer.start();

    // Enable non-blocking input
#ifndef _WIN32
    set_nonblocking_input(true);
#endif

    // Main thread loop to display progress and handle input
    while (true) {
        bool completed;
        {
            std::lock_guard<std::mutex> lock(proc_ctx_mutex);
            completed = proc_ctx.completed;
        }
        if (completed) {
            break;
        }

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
            {
                std::lock_guard<std::mutex> lock(proc_ctx_mutex);
                proc_ctx.pause = !proc_ctx.pause;
                if (proc_ctx.pause) {
                    putchar('\n');
                    spdlog::info("Processing paused. Press SPACE to resume, 'q' to abort.");
                    timer.pause();
                } else {
                    spdlog::info("Resuming processing...");
                    timer.resume();
                }
            }
        } else if (ch == 'q' || ch == 'Q') {
            // Abort processing
            putchar('\n');
            spdlog::info("Aborting processing...");
            {
                std::lock_guard<std::mutex> lock(proc_ctx_mutex);
                proc_ctx.abort = true;
                newline_required = false;
            }
            break;
        }

        // Display progress
        if (!arguments.noprogress) {
            int64_t processed_frames, total_frames;
            bool pause;
            {
                std::lock_guard<std::mutex> lock(proc_ctx_mutex);
                processed_frames = proc_ctx.processed_frames;
                total_frames = proc_ctx.total_frames;
                pause = proc_ctx.pause;
            }
            if (!pause && total_frames > 0) {
                double percentage = total_frames > 0 ? static_cast<double>(processed_frames) *
                                                           100.0 / static_cast<double>(total_frames)
                                                     : 0.0;
                int64_t time_elapsed = timer.get_elapsed_time() / 1000;
                std::cout << "\rProcessing frame " << processed_frames << "/" << total_frames
                          << " (" << percentage << "%); time elapsed: " << time_elapsed << "s";
                std::cout.flush();
                newline_required = true;
            }
        }

        // Sleep for 100ms
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Restore terminal to blocking mode
#ifndef _WIN32
    set_nonblocking_input(false);
#endif

    // Join the processing thread to ensure it completes before exiting
    processing_thread.join();

    // Print a newline if progress bar was displayed
    if (newline_required) {
        std::cout << '\n';
    }

    // Print final message based on processing result
    bool aborted;
    {
        std::lock_guard<std::mutex> lock(proc_ctx_mutex);
        aborted = proc_ctx.abort;
    }
    if (aborted) {
        spdlog::warn("Video processing aborted");
        return 2;
    } else if (proc_ret != 0) {
        spdlog::error("Video processing failed with error code {}", proc_ret);
        return 1;
    } else {
        spdlog::info("Video processed successfully");
    }

    // Calculate statistics
    int64_t processed_frames;
    {
        std::lock_guard<std::mutex> lock(proc_ctx_mutex);
        processed_frames = proc_ctx.processed_frames;
    }
    int64_t time_elapsed = timer.get_elapsed_time() / 1000;
    float average_speed_fps = static_cast<float>(processed_frames) /
                              (time_elapsed > 0 ? static_cast<float>(time_elapsed) : 1);

    // Print processing summary
    printf("====== Video2X %s summary ======\n", arguments.benchmark ? "Benchmark" : "Processing");
    printf("Video file processed: %s\n", arguments.in_fname.u8string().c_str());
    printf("Total frames processed: %ld\n", proc_ctx.processed_frames);
    printf("Total time taken: %ld s\n", time_elapsed);
    printf("Average processing speed: %.2f FPS\n", average_speed_fps);

    // Print additional information if not in benchmark mode
    if (!arguments.benchmark) {
        printf("Output written to: %s\n", arguments.out_fname.u8string().c_str());
    }

    return 0;
}
