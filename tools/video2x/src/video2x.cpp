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
#include <vulkan/vulkan.h>

#ifdef _WIN32
#define BOOST_PROGRAM_OPTIONS_WCHAR_T
#define PO_STR_VALUE po::wvalue
#else
#define PO_STR_VALUE po::value
#endif
#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include "libvideo2x/char_defs.h"
#include "timer.h"

// Indicate if a newline needs to be printed before the next output
std::atomic<bool> newline_required = false;

// Mutex for synchronizing access to VideoProcessingContext
std::mutex proc_ctx_mutex;

// Structure to hold parsed arguments
struct Arguments {
    StringType loglevel = STR("info");
    bool noprogress = false;

    // General options
    std::filesystem::path in_fname;
    std::filesystem::path out_fname;
    StringType filter_type;
    uint32_t gpuid = 0;
    StringType hwaccel = STR("none");
    bool nocopystreams = false;
    bool benchmark = false;

    // Encoder options
    StringType codec = STR("libx264");
    StringType preset = STR("slow");
    StringType pix_fmt;
    int64_t bitrate = 0;
    float crf = 20.0f;

    // libplacebo options
    std::filesystem::path shader_path;
    int out_width = 0;
    int out_height = 0;

    // RealESRGAN options
    StringType model_name;
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

bool is_valid_realesrgan_model(const StringType &model) {
    static const std::unordered_set<StringType> valid_realesrgan_models = {
        STR("realesrgan-plus"), STR("realesrgan-plus-anime"), STR("realesr-animevideov3")
    };
    return valid_realesrgan_models.count(model) > 0;
}

enum Libvideo2xLogLevel parse_log_level(const StringType &level_name) {
    if (level_name == STR("trace")) {
        return LIBVIDEO2X_LOG_LEVEL_TRACE;
    } else if (level_name == STR("debug")) {
        return LIBVIDEO2X_LOG_LEVEL_DEBUG;
    } else if (level_name == STR("info")) {
        return LIBVIDEO2X_LOG_LEVEL_INFO;
    } else if (level_name == STR("warning") || level_name == STR("warn")) {
        return LIBVIDEO2X_LOG_LEVEL_WARNING;
    } else if (level_name == STR("error")) {
        return LIBVIDEO2X_LOG_LEVEL_ERROR;
    } else if (level_name == STR("critical")) {
        return LIBVIDEO2X_LOG_LEVEL_CRITICAL;
    } else if (level_name == STR("off") || level_name == STR("none")) {
        return LIBVIDEO2X_LOG_LEVEL_OFF;
    } else {
        spdlog::warn("Invalid log level specified. Defaulting to 'info'.");
        return LIBVIDEO2X_LOG_LEVEL_INFO;
    }
}

int list_gpus() {
    // Create a Vulkan instance
    VkInstance instance;
    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS) {
        spdlog::critical("Failed to create Vulkan instance.");
        return -1;
    }

    // Enumerate physical devices
    uint32_t device_count = 0;
    VkResult result = vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    if (result != VK_SUCCESS) {
        spdlog::critical("Failed to enumerate Vulkan physical devices.");
        vkDestroyInstance(instance, nullptr);
        return -1;
    }

    // Check if any devices are found
    if (device_count == 0) {
        spdlog::critical("No Vulkan physical devices found.");
        vkDestroyInstance(instance, nullptr);
        return -1;
    }

    // Get physical device properties
    std::vector<VkPhysicalDevice> physical_devices(device_count);
    result = vkEnumeratePhysicalDevices(instance, &device_count, physical_devices.data());
    if (result != VK_SUCCESS) {
        spdlog::critical("Failed to enumerate Vulkan physical devices.");
        vkDestroyInstance(instance, nullptr);
        return -1;
    }

    // List GPU information
    for (uint32_t i = 0; i < device_count; i++) {
        VkPhysicalDevice device = physical_devices[i];
        VkPhysicalDeviceProperties device_properties;
        vkGetPhysicalDeviceProperties(device, &device_properties);

        // Print GPU ID and name
        std::cout << i << ". " << device_properties.deviceName << std::endl;
        std::cout << "\tType: ";
        switch (device_properties.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                std::cout << "Integrated GPU";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                std::cout << "Discrete GPU";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                std::cout << "Virtual GPU";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                std::cout << "CPU";
                break;
            default:
                std::cout << "Unknown";
                break;
        }
        std::cout << std::endl;

        // Print Vulkan API version
        std::cout << "\tVulkan API Version: " << VK_VERSION_MAJOR(device_properties.apiVersion)
                  << "." << VK_VERSION_MINOR(device_properties.apiVersion) << "."
                  << VK_VERSION_PATCH(device_properties.apiVersion) << std::endl;

        // Print driver version
        std::cout << "\tDriver Version: " << VK_VERSION_MAJOR(device_properties.driverVersion)
                  << "." << VK_VERSION_MINOR(device_properties.driverVersion) << "."
                  << VK_VERSION_PATCH(device_properties.driverVersion) << std::endl;
    }

    // Clean up Vulkan instance
    vkDestroyInstance(instance, nullptr);
    return 0;
}

int is_valid_gpu_id(uint32_t gpu_id) {
    // Create a Vulkan instance
    VkInstance instance;
    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS) {
        spdlog::error("Failed to create Vulkan instance.");
        return -1;
    }

    // Enumerate physical devices
    uint32_t device_count = 0;
    VkResult result = vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    if (result != VK_SUCCESS) {
        spdlog::error("Failed to enumerate Vulkan physical devices.");
        vkDestroyInstance(instance, nullptr);
        return -1;
    }

    // Clean up Vulkan instance
    vkDestroyInstance(instance, nullptr);

    if (gpu_id >= device_count) {
        return 0;
    }
    return 1;
}

// Wrapper function for video processing thread
void process_video_thread(
    Arguments *arguments,
    int *proc_ret,
    AVHWDeviceType hw_device_type,
    FilterConfig *filter_config,
    EncoderConfig *encoder_config,
    VideoProcessingContext *proc_ctx
) {
    enum Libvideo2xLogLevel log_level = parse_log_level(arguments->loglevel);

    StringType in_fname_string;
    StringType out_fname_string;

#ifdef _WIN32
    in_fname_string = StringType(arguments->in_fname.wstring());
    out_fname_string = StringType(arguments->out_fname.wstring());
#else
    in_fname_string = StringType(arguments->in_fname.string());
    out_fname_string = StringType(arguments->out_fname.string());
#endif

    const CharType *in_fname = in_fname_string.c_str();
    const CharType *out_fname = out_fname_string.c_str();

    *proc_ret = process_video(
        in_fname,
        out_fname,
        log_level,
        arguments->benchmark,
        arguments->gpuid,
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
    // Set console output code page to UTF-8
    SetConsoleOutputCP(CP_UTF8);

    // Enable ANSI escape codes
    HANDLE console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD console_mode = 0;
    GetConsoleMode(console_handle, &console_mode);
    console_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(console_handle, console_mode);
#else
int main(int argc, char **argv) {
#endif
    // Initialize arguments structure
    Arguments arguments;

    // Parse command line arguments using Boost.Program_options
    try {
        po::options_description desc("Allowed options");

        desc.add_options()
            ("help", "Display this help page")
            ("version,v", "Print program version")
            ("loglevel", PO_STR_VALUE<StringType>(&arguments.loglevel)->default_value(STR("info"), "info"), "Set log level (trace, debug, info, warn, error, critical, none)")
            ("noprogress", po::bool_switch(&arguments.noprogress), "Do not display the progress bar")
            ("listgpus", "List the available GPUs")

            // General Processing Options
            ("input,i", PO_STR_VALUE<StringType>(), "Input video file path")
            ("output,o", PO_STR_VALUE<StringType>(), "Output video file path")
            ("filter,f", PO_STR_VALUE<StringType>(&arguments.filter_type), "Filter to use: 'libplacebo' or 'realesrgan'")
            ("gpuid,g", po::value<uint32_t>(&arguments.gpuid)->default_value(0), "Vulkan GPU ID (default: 0)")
            ("hwaccel,a", PO_STR_VALUE<StringType>(&arguments.hwaccel)->default_value(STR("none"), "none"), "Hardware acceleration method (default: none)")
            ("nocopystreams", po::bool_switch(&arguments.nocopystreams), "Do not copy audio and subtitle streams")
            ("benchmark", po::bool_switch(&arguments.benchmark), "Discard processed frames and calculate average FPS")

            // Encoder options
            ("codec,c", PO_STR_VALUE<StringType>(&arguments.codec)->default_value(STR("libx264"), "libx264"), "Output codec (default: libx264)")
            ("preset,p", PO_STR_VALUE<StringType>(&arguments.preset)->default_value(STR("slow"), "slow"), "Encoder preset (default: slow)")
            ("pixfmt,x", PO_STR_VALUE<StringType>(&arguments.pix_fmt), "Output pixel format (default: auto)")
            ("bitrate,b", po::value<int64_t>(&arguments.bitrate)->default_value(0), "Bitrate in bits per second (default: 0 (VBR))")
            ("crf,q", po::value<float>(&arguments.crf)->default_value(20.0f), "Constant Rate Factor (default: 20.0)")

            // libplacebo options
            ("shader,s", PO_STR_VALUE<StringType>(), "Name or path of the GLSL shader file to use")
            ("width,w", po::value<int>(&arguments.out_width), "Output width")
            ("height,h", po::value<int>(&arguments.out_height), "Output height")

            // RealESRGAN options
            ("model,m", PO_STR_VALUE<StringType>(&arguments.model_name), "Name of the model to use")
            ("scale,r", po::value<int>(&arguments.scaling_factor), "Scaling factor (2, 3, or 4)")
        ;

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

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        }

        if (vm.count("version")) {
            std::cout << "Video2X version " << LIBVIDEO2X_VERSION_STRING << std::endl;
            return 0;
        }

        if (vm.count("listgpus")) {
            return list_gpus();
        }

        // Assign positional arguments
        if (vm.count("input")) {
            arguments.in_fname = std::filesystem::path(vm["input"].as<StringType>());
        } else {
            spdlog::critical("Input file path is required.");
            return 1;
        }

        if (vm.count("output")) {
            arguments.out_fname = std::filesystem::path(vm["output"].as<StringType>());
        } else if (!arguments.benchmark) {
            spdlog::critical("Output file path is required.");
            return 1;
        }

        if (!vm.count("filter")) {
            spdlog::critical("Filter type is required (libplacebo or realesrgan).");
            return 1;
        }

        if (vm.count("shader")) {
            arguments.shader_path = std::filesystem::path(vm["shader"].as<StringType>());
        }

        if (vm.count("model")) {
            if (!is_valid_realesrgan_model(vm["model"].as<StringType>())) {
                spdlog::critical(
                    "Invalid model specified. Must be 'realesrgan-plus', "
                    "'realesrgan-plus-anime', or 'realesr-animevideov3'."
                );
                return 1;
            }
        }
    } catch (const po::error &e) {
        spdlog::critical("Error parsing options: {}", e.what());
        return 1;
    } catch (const std::exception &e) {
        spdlog::critical("Unexpected exception caught while parsing options: {}", e.what());
        return 1;
    }

    // Additional validations
    if (arguments.filter_type == STR("libplacebo")) {
        if (arguments.shader_path.empty() || arguments.out_width == 0 ||
            arguments.out_height == 0) {
            spdlog::critical(
                "For libplacebo, shader name/path (-s), width (-w), "
                "and height (-h) are required."
            );
            return 1;
        }
    } else if (arguments.filter_type == STR("realesrgan")) {
        if (arguments.scaling_factor == 0 || arguments.model_name.empty()) {
            spdlog::critical("For realesrgan, scaling factor (-r) and model (-m) are required.");
            return 1;
        }
        if (arguments.scaling_factor != 2 && arguments.scaling_factor != 3 &&
            arguments.scaling_factor != 4) {
            spdlog::critical("Scaling factor must be 2, 3, or 4.");
            return 1;
        }
    } else {
        spdlog::critical("Invalid filter type specified. Must be 'libplacebo' or 'realesrgan'.");
        return 1;
    }

    // Validate GPU ID
    int gpu_status = is_valid_gpu_id(arguments.gpuid);
    if (gpu_status < 0) {
        spdlog::warn("Unable to validate GPU ID.");
    } else if (arguments.gpuid > 0 && gpu_status == 0) {
        spdlog::critical("Invalid GPU ID specified.");
        return 1;
    }

    // Validate bitrate
    if (arguments.bitrate < 0) {
        spdlog::critical("Invalid bitrate specified.");
        return 1;
    }

    // Validate CRF
    if (arguments.crf < 0.0f || arguments.crf > 51.0f) {
        spdlog::critical("CRF must be between 0 and 51.");
        return 1;
    }

    // Parse codec to AVCodec
    const AVCodec *codec = avcodec_find_encoder_by_name(wstring_to_utf8(arguments.codec).c_str());
    if (!codec) {
        spdlog::critical("Codec '{}' not found.", wstring_to_utf8(arguments.codec));
        return 1;
    }

    // Parse pixel format to AVPixelFormat
    enum AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;
    if (!arguments.pix_fmt.empty()) {
        pix_fmt = av_get_pix_fmt(wstring_to_utf8(arguments.pix_fmt).c_str());
        if (pix_fmt == AV_PIX_FMT_NONE) {
            spdlog::critical("Invalid pixel format '{}'.", wstring_to_utf8(arguments.pix_fmt));
            return 1;
        }
    }

    // Set spdlog log level
    auto log_level = parse_log_level(arguments.loglevel);
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

    // Print program version and processing information
    spdlog::info("Video2X version {}", LIBVIDEO2X_VERSION_STRING);
    spdlog::info("Processing file: {}", arguments.in_fname.u8string());

#ifdef _WIN32
    std::wstring shader_path_str = arguments.shader_path.wstring();
#else
    std::string shader_path_str = arguments.shader_path.string();
#endif

    // Setup filter configurations based on the parsed arguments
    FilterConfig filter_config;
    if (arguments.filter_type == STR("libplacebo")) {
        filter_config.filter_type = FILTER_LIBPLACEBO;
        filter_config.config.libplacebo.out_width = arguments.out_width;
        filter_config.config.libplacebo.out_height = arguments.out_height;
        filter_config.config.libplacebo.shader_path = shader_path_str.c_str();
    } else if (arguments.filter_type == STR("realesrgan")) {
        filter_config.filter_type = FILTER_REALESRGAN;
        filter_config.config.realesrgan.tta_mode = false;
        filter_config.config.realesrgan.scaling_factor = arguments.scaling_factor;
        filter_config.config.realesrgan.model_name = arguments.model_name.c_str();
    }

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
    if (arguments.hwaccel != STR("none")) {
        hw_device_type = av_hwdevice_find_type_by_name(wstring_to_utf8(arguments.hwaccel).c_str());
        if (hw_device_type == AV_HWDEVICE_TYPE_NONE) {
            spdlog::critical(
                "Invalid hardware device type '{}'.", wstring_to_utf8(arguments.hwaccel)
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
    spdlog::info("Press [space] to pause/resume, [q] to abort.");

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
                    std::cout
                        << "\r\033[KProcessing paused; press [space] to resume, [q] to abort.";
                    std::cout.flush();
                    timer.pause();
                } else {
                    std::cout << "\r\033[KProcessing resumed.";
                    std::cout.flush();
                    timer.resume();
                }
                newline_required = true;
            }
        } else if (ch == 'q' || ch == 'Q') {
            // Abort processing
            if (newline_required) {
                putchar('\n');
            }
            spdlog::warn("Aborting gracefully; press Ctrl+C to terminate forcefully.");
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
            if (!pause && (total_frames > 0 || processed_frames > 0)) {
                double percentage = total_frames > 0 ? static_cast<double>(processed_frames) *
                                                           100.0 / static_cast<double>(total_frames)
                                                     : 0.0;
                int time_elapsed = static_cast<int>(timer.get_elapsed_time() / 1000);

                // Calculate hours, minutes, and seconds elapsed
                int hours_elapsed = time_elapsed / 3600;
                int minutes_elapsed = (time_elapsed % 3600) / 60;
                int seconds_elapsed = time_elapsed % 60;

                // Calculate estimated time remaining
                int64_t frames_remaining = total_frames - processed_frames;
                double processing_rate = static_cast<double>(processed_frames) / time_elapsed;
                int time_remaining =
                    static_cast<int>(static_cast<double>(frames_remaining) / processing_rate);
                time_remaining = std::max<int>(time_remaining, 0);

                // Calculate hours, minutes, and seconds remaining
                int hours_remaining = time_remaining / 3600;
                int minutes_remaining = (time_remaining % 3600) / 60;
                int seconds_remaining = time_remaining % 60;

                // Print the progress bar
                std::cout << "\r\033[Kframe=" << processed_frames << "/" << total_frames << " ("
                          << std::fixed << std::setprecision(2) << percentage
                          << "%); fps=" << std::fixed << std::setprecision(2) << processing_rate
                          << "; elapsed=" << std::setw(2) << std::setfill('0') << hours_elapsed
                          << ":" << std::setw(2) << std::setfill('0') << minutes_elapsed << ":"
                          << std::setw(2) << std::setfill('0') << seconds_elapsed
                          << "; remaining=" << std::setw(2) << std::setfill('0') << hours_remaining
                          << ":" << std::setw(2) << std::setfill('0') << minutes_remaining << ":"
                          << std::setw(2) << std::setfill('0') << seconds_remaining;
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
        spdlog::critical("Video processing failed with error code {}", proc_ret);
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
