#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
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
    StringType log_level = STR("info");
    bool no_progress = false;

    // General options
    std::filesystem::path in_fname;
    std::filesystem::path out_fname;
    StringType processor_type;
    uint32_t gpu_id = 0;
    StringType hwaccel = STR("none");
    bool no_copy_streams = false;
    bool benchmark = false;

    // Encoder options
    StringType codec = STR("libx264");
    StringType pix_fmt;
    int64_t bit_rate = 0;
    int rc_buffer_size = 0;
    int rc_min_rate = 0;
    int rc_max_rate = 0;
    int qmin = -1;
    int qmax = -1;
    int gop_size = -1;
    int max_b_frames = -1;
    int keyint_min = -1;
    int refs = -1;
    int thread_count = 0;
    int delay = 0;
    std::vector<std::pair<StringType, StringType>> extra_options;

    // libplacebo options
    std::filesystem::path shader_path;
    int width = 0;
    int height = 0;

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
    ProcessorConfig *filter_config,
    EncoderConfig *encoder_config,
    VideoProcessingContext *proc_ctx
) {
    enum Libvideo2xLogLevel log_level = parse_log_level(arguments->log_level);

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
        arguments->gpu_id,
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
        // clang-format off
        po::options_description all_opts("General options");
        all_opts.add_options()
            ("help", "Display this help page")
            ("version,V", "Print program version and exit")
            ("verbose,v", PO_STR_VALUE<StringType>(&arguments.log_level)->default_value(STR("info"),
                "info"), "Set verbosity level (trace, debug, info, warn, error, critical, none)")
            ("no-progress", po::bool_switch(&arguments.no_progress),
                "Do not display the progress bar")
            ("list-gpus,l", "List the available GPUs")

            // General Processing Options
            ("input,i", PO_STR_VALUE<StringType>(), "Input video file path")
            ("output,o", PO_STR_VALUE<StringType>(), "Output video file path")
            ("processor,p", PO_STR_VALUE<StringType>(&arguments.processor_type),
                "Processor to use: 'libplacebo', 'realesrgan', or 'rife'")
            ("gpu,g", po::value<uint32_t>(&arguments.gpu_id)->default_value(0),
                "GPU ID (Vulkan device index)")
            ("hwaccel,a", PO_STR_VALUE<StringType>(&arguments.hwaccel)->default_value(STR("none"),
                "none"), "Hardware acceleration method (mostly for decoding)")
            ("benchmark,b", po::bool_switch(&arguments.benchmark),
                "Discard processed frames and calculate average FPS; "
                "useful for detecting encoder bottlenecks")
        ;

        po::options_description encoder_opts("Encoder options");
        encoder_opts.add_options()
            // Encoder options
            ("codec,c", PO_STR_VALUE<StringType>(&arguments.codec)->default_value(STR("libx264"),
                "libx264"), "Output codec")
            ("no-copy-streams", po::bool_switch(&arguments.no_copy_streams),
                "Do not copy audio and subtitle streams")
            ("pix-fmt", PO_STR_VALUE<StringType>(&arguments.pix_fmt), "Output pixel format")
            ("bit-rate", po::value<int64_t>(&arguments.bit_rate)->default_value(0),
                "Bitrate in bits per second")
            ("rc-buffer-size", po::value<int>(&arguments.rc_buffer_size)->default_value(0),
                "Rate control buffer size in bits")
            ("rc-min-rate", po::value<int>(&arguments.rc_min_rate)->default_value(0),
                "Minimum rate control")
            ("rc-max-rate", po::value<int>(&arguments.rc_max_rate)->default_value(0),
                "Maximum rate control")
            ("qmin", po::value<int>(&arguments.qmin)->default_value(-1), "Minimum quantizer")
            ("qmax", po::value<int>(&arguments.qmax)->default_value(-1), "Maximum quantizer")
            ("gop-size", po::value<int>(&arguments.gop_size)->default_value(-1),
                "Group of pictures structure size")
            ("max-b-frames", po::value<int>(&arguments.max_b_frames)->default_value(-1),
                "Maximum number of B-frames")
            ("keyint-min", po::value<int>(&arguments.keyint_min)->default_value(-1),
                "Minimum interval between keyframes")
            ("refs", po::value<int>(&arguments.refs)->default_value(-1),
                "Number of reference frames")
            ("thread-count", po::value<int>(&arguments.thread_count)->default_value(0),
                "Number of threads for encoding")
            ("delay", po::value<int>(&arguments.delay)->default_value(0),
                "Delay in milliseconds for encoder")

            // Extra encoder options (key-value pairs)
            ("extra-encoder-option,e", PO_STR_VALUE<std::vector<StringType>>()->multitoken(),
                "Additional AVOption(s) for the encoder (format: -e key=value)")
            ;

        po::options_description libplacebo_opts("libplacebo options");
        libplacebo_opts.add_options()
            ("shader,s", PO_STR_VALUE<StringType>(), "Name/path of the GLSL shader file to use")
            ("width,w", po::value<int>(&arguments.width), "Output width")
            ("height,h", po::value<int>(&arguments.height), "Output height")
        ;

        // RealESRGAN options
        po::options_description realesrgan_opts("RealESRGAN options");
        realesrgan_opts.add_options()
            ("model,m", PO_STR_VALUE<StringType>(&arguments.model_name), "Name of the model to use")
            ("scale,r", po::value<int>(&arguments.scaling_factor), "Scaling factor (2, 3, or 4)")
        ;
        // clang-format on

        // Combine all options
        all_opts.add(encoder_opts).add(libplacebo_opts).add(realesrgan_opts);

        // Positional arguments
        po::positional_options_description p;
        p.add("input", 1).add("output", 1).add("filter", 1);

#ifdef _WIN32
        po::variables_map vm;
        po::store(po::wcommand_line_parser(argc, argv).options(all_opts).positional(p).run(), vm);
#else
        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(all_opts).positional(p).run(), vm);
#endif
        po::notify(vm);

        if (vm.count("help") || argc == 1) {
            std::cout << all_opts << std::endl;
            std::cout
                << "Examples:" << std::endl
                << "  Upscale an anime video to 4K using libplacebo:" << std::endl
                << "    video2x -i input.mp4 -o output.mp4 -f libplacebo -s anime4k-v4-a+a "
                   "-w 3840 -h 2160"
                << std::endl
                << std::endl
                << "  Upscale a film video by 4x using RealESRGAN with custom encoder options"
                << std::endl
                << "    video2x -i input.mkv -o output.mkv -f realesrgan -m realesrgan-plus -r 4 \\"
                << std::endl
                << "      -c libx264rgb -e crf=17 -e preset=veryslow -e tune=film" << std::endl;
            return 0;
        }

        if (vm.count("version")) {
            std::cout << "Video2X version " << LIBVIDEO2X_VERSION_STRING << std::endl;
            return 0;
        }

        if (vm.count("list-gpus")) {
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

        if (!vm.count("processor")) {
            spdlog::critical("Processor type is required (libplacebo, realesrgan, or rife).");
            return 1;
        }

        // Parse avoptions
        if (vm.count("extra-encoder-option")) {
            for (const auto &opt : vm["extra-encoder-option"].as<std::vector<StringType>>()) {
                size_t eq_pos = opt.find('=');
                if (eq_pos != StringType::npos) {
                    StringType key = opt.substr(0, eq_pos);
                    StringType value = opt.substr(eq_pos + 1);
                    arguments.extra_options.push_back(std::make_pair(key, value));
                } else {
                    spdlog::critical("Invalid extra AVOption format: {}", wstring_to_utf8(opt));
                    return 1;
                }
            }
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
    if (arguments.processor_type == STR("libplacebo")) {
        if (arguments.shader_path.empty() || arguments.width == 0 || arguments.height == 0) {
            spdlog::critical(
                "For libplacebo, shader name/path (-s), width (-w), "
                "and height (-h) are required."
            );
            return 1;
        }
    } else if (arguments.processor_type == STR("realesrgan")) {
        if (arguments.scaling_factor == 0 || arguments.model_name.empty()) {
            spdlog::critical("For realesrgan, scaling factor (-r) and model (-m) are required.");
            return 1;
        }
        if (arguments.scaling_factor != 2 && arguments.scaling_factor != 3 &&
            arguments.scaling_factor != 4) {
            spdlog::critical("Scaling factor must be 2, 3, or 4.");
            return 1;
        }
    } else if (arguments.processor_type == STR("rife")) {
        // TODO: Complete RIFE validation
        ;
    } else {
        spdlog::critical(
            "Invalid processor type specified. Must be 'libplacebo', 'realesrgan', or 'rife'."
        );
        return 1;
    }

    // Validate GPU ID
    int gpu_status = is_valid_gpu_id(arguments.gpu_id);
    if (gpu_status < 0) {
        spdlog::warn("Unable to validate GPU ID.");
    } else if (arguments.gpu_id > 0 && gpu_status == 0) {
        spdlog::critical("Invalid GPU ID specified.");
        return 1;
    }

    // Validate bitrate
    if (arguments.bit_rate < 0) {
        spdlog::critical("Invalid bitrate specified.");
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
    auto log_level = parse_log_level(arguments.log_level);
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
    ProcessorConfig processor_config;
    if (arguments.processor_type == STR("libplacebo")) {
        processor_config.processor_type = PROCESSOR_LIBPLACEBO;
        processor_config.config.libplacebo.width = arguments.width;
        processor_config.config.libplacebo.height = arguments.height;
        processor_config.config.libplacebo.shader_path = shader_path_str.c_str();
    } else if (arguments.processor_type == STR("realesrgan")) {
        processor_config.processor_type = PROCESSOR_REALESRGAN;
        processor_config.config.realesrgan.tta_mode = false;
        processor_config.config.realesrgan.scaling_factor = arguments.scaling_factor;
        processor_config.config.realesrgan.model_name = arguments.model_name.c_str();
    } else if (arguments.processor_type == STR("rife")) {
        processor_config.processor_type = PROCESSOR_RIFE;
        processor_config.config.rife.tta_mode = false;
        processor_config.config.rife.tta_temporal_mode = false;
        processor_config.config.rife.uhd_mode = false;
        processor_config.config.rife.num_threads = 0;
        processor_config.config.rife.rife_v2 = false;
        processor_config.config.rife.rife_v4 = true;
        processor_config.config.rife.model_name = STR("rife-v4.6");
    }

    // Setup encoder configuration
    EncoderConfig encoder_config;
    encoder_config.codec = codec->id;
    encoder_config.copy_streams = !arguments.no_copy_streams;
    encoder_config.width = arguments.width;
    encoder_config.height = arguments.height;
    encoder_config.pix_fmt = pix_fmt;
    encoder_config.bit_rate = arguments.bit_rate;
    encoder_config.rc_buffer_size = arguments.rc_buffer_size;
    encoder_config.rc_max_rate = arguments.rc_max_rate;
    encoder_config.rc_min_rate = arguments.rc_min_rate;
    encoder_config.qmin = arguments.qmin;
    encoder_config.qmax = arguments.qmax;
    encoder_config.gop_size = arguments.gop_size;
    encoder_config.max_b_frames = arguments.max_b_frames;
    encoder_config.keyint_min = arguments.keyint_min;
    encoder_config.refs = arguments.refs;
    encoder_config.thread_count = arguments.thread_count;
    encoder_config.delay = arguments.delay;

    // Handle extra AVOptions
    encoder_config.nb_extra_options = arguments.extra_options.size();
    encoder_config.extra_options = static_cast<decltype(encoder_config.extra_options)>(malloc(
        static_cast<unsigned long>(encoder_config.nb_extra_options + 1) *
        sizeof(encoder_config.extra_options[0])
    ));
    if (encoder_config.extra_options == nullptr) {
        spdlog::critical("Failed to allocate memory for extra AVOptions.");
        return 1;
    }

    // Copy extra AVOptions to the encoder configuration
    for (size_t i = 0; i < encoder_config.nb_extra_options; i++) {
        const std::string key = wstring_to_utf8(arguments.extra_options[i].first);
        const std::string value = wstring_to_utf8(arguments.extra_options[i].second);
        encoder_config.extra_options[i].key = strdup(key.c_str());
        encoder_config.extra_options[i].value = strdup(value.c_str());
    }

    // Custom deleter for extra AVOptions
    auto extra_options_deleter = [&](decltype(encoder_config.extra_options) *extra_options_ptr) {
        auto extra_options = *extra_options_ptr;
        for (size_t i = 0; i < encoder_config.nb_extra_options; i++) {
            free(const_cast<char *>(extra_options[i].key));
            free(const_cast<char *>(extra_options[i].value));
        }
        free(extra_options);
        *extra_options_ptr = nullptr;
    };

    // Define a unique_ptr to automatically free extra_options
    std::unique_ptr<decltype(encoder_config.extra_options), decltype(extra_options_deleter)>
        extra_options_guard(&encoder_config.extra_options, extra_options_deleter);

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
        &processor_config,
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
        if (!arguments.no_progress) {
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
    int time_elapsed = static_cast<int>(timer.get_elapsed_time() / 1000);
    int hours_elapsed = time_elapsed / 3600;
    int minutes_elapsed = (time_elapsed % 3600) / 60;
    int seconds_elapsed = time_elapsed % 60;
    float average_speed_fps = static_cast<float>(processed_frames) /
                              (time_elapsed > 0 ? static_cast<float>(time_elapsed) : 1);

    // Print processing summary
    std::cout << "====== Video2X " << (arguments.benchmark ? "Benchmark" : "Processing")
              << " summary ======" << std::endl;
    std::cout << "Video file processed: " << arguments.in_fname.u8string() << std::endl;
    std::cout << "Total frames processed: " << processed_frames << std::endl;
    std::cout << "Total time taken: " << std::setw(2) << std::setfill('0') << hours_elapsed << ":"
              << std::setw(2) << std::setfill('0') << minutes_elapsed << ":" << std::setw(2)
              << std::setfill('0') << seconds_elapsed << std::endl;
    std::cout << "Average processing speed: " << std::fixed << std::setprecision(2)
              << average_speed_fps << " FPS" << std::endl;

    // Print additional information if not in benchmark mode
    if (!arguments.benchmark) {
        std::cout << "Output written to: " << arguments.out_fname.u8string() << std::endl;
    }

    return 0;
}
