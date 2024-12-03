#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
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
}

#include <libvideo2x/libvideo2x.h>
#include <libvideo2x/version.h>
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

#include "timer.h"

// Indicate if a newline needs to be printed before the next output
std::atomic<bool> newline_required = false;

// Structure to hold parsed arguments
struct Arguments {
    Video2xLogLevel log_level = Video2xLogLevel::Info;
    bool no_progress = false;

    // General options
    std::filesystem::path in_fname;
    std::filesystem::path out_fname;
    StringType processor_type;
    StringType hwaccel = STR("none");
    uint32_t vk_device_index = 0;
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
    std::vector<std::pair<StringType, StringType>> extra_encoder_opts;

    // General processing options
    int width = 0;
    int height = 0;
    int scaling_factor = 0;
    int frm_rate_mul = 2;
    float scn_det_thresh = 0.0f;

    // libplacebo options
    StringType libplacebo_shader_path;

    // RealESRGAN options
    StringType realesrgan_model_name = STR("realesr-animevideov3");

    // RIFE options
    StringType rife_model_name = STR("rife-v4.6");
    bool rife_uhd_mode = false;
};

// Set UNIX terminal input to non-blocking mode
#ifndef _WIN32
void set_nonblocking_input(bool enable) {
    static termios oldt, newt;
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

#ifdef _WIN32
std::string wstring_to_u8string(const std::wstring &wstr) {
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
std::string wstring_to_u8string(const std::string &str) {
    return str;
}
#endif

void set_spdlog_level(Video2xLogLevel log_level) {
    switch (log_level) {
        case Video2xLogLevel::Trace:
            spdlog::set_level(spdlog::level::trace);
            break;
        case Video2xLogLevel::Debug:
            spdlog::set_level(spdlog::level::debug);
            break;
        case Video2xLogLevel::Info:
            spdlog::set_level(spdlog::level::info);
            break;
        case Video2xLogLevel::Warning:
            spdlog::set_level(spdlog::level::warn);
            break;
        case Video2xLogLevel::Error:
            spdlog::set_level(spdlog::level::err);
            break;
        case Video2xLogLevel::Critical:
            spdlog::set_level(spdlog::level::critical);
            break;
        case Video2xLogLevel::Off:
            spdlog::set_level(spdlog::level::off);
            break;
        default:
            spdlog::set_level(spdlog::level::info);
            break;
    }
}

std::optional<Video2xLogLevel> find_log_level_by_name(const StringType &log_level_name) {
    // Static map to store the mapping
    static const std::unordered_map<StringType, Video2xLogLevel> log_level_map = {
        {STR("trace"), Video2xLogLevel::Trace},
        {STR("debug"), Video2xLogLevel::Debug},
        {STR("info"), Video2xLogLevel::Info},
        {STR("warning"), Video2xLogLevel::Warning},
        {STR("warn"), Video2xLogLevel::Warning},
        {STR("error"), Video2xLogLevel::Error},
        {STR("critical"), Video2xLogLevel::Critical},
        {STR("off"), Video2xLogLevel::Off},
        {STR("none"), Video2xLogLevel::Off}
    };

    // Normalize the input to lowercase
    StringType normalized_name = log_level_name;
    std::transform(
        normalized_name.begin(), normalized_name.end(), normalized_name.begin(), ::tolower
    );

    // Lookup the log level in the map
    auto it = log_level_map.find(normalized_name);
    if (it != log_level_map.end()) {
        return it->second;
    }

    return std::nullopt;
}

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

bool is_valid_rife_model(const StringType &model) {
    static const std::unordered_set<StringType> valid_realesrgan_models = {
        STR("rife"),
        STR("rife-HD"),
        STR("rife-UHD"),
        STR("rife-anime"),
        STR("rife-v2"),
        STR("rife-v2.3"),
        STR("rife-v2.4"),
        STR("rife-v3.0"),
        STR("rife-v3.1"),
        STR("rife-v4"),
        STR("rife-v4.6"),
    };
    return valid_realesrgan_models.count(model) > 0;
}

int enumerate_vulkan_devices(VkInstance *instance, std::vector<VkPhysicalDevice> &devices) {
    // Create a Vulkan instance
    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    VkResult result = vkCreateInstance(&create_info, nullptr, instance);
    if (result != VK_SUCCESS) {
        spdlog::error("Failed to create Vulkan instance.");
        return -1;
    }

    // Enumerate physical devices
    uint32_t device_count = 0;
    result = vkEnumeratePhysicalDevices(*instance, &device_count, nullptr);
    if (result != VK_SUCCESS || device_count == 0) {
        spdlog::error("Failed to enumerate Vulkan physical devices or no devices available.");
        vkDestroyInstance(*instance, nullptr);
        return -1;
    }

    devices.resize(device_count);
    result = vkEnumeratePhysicalDevices(*instance, &device_count, devices.data());
    if (result != VK_SUCCESS) {
        spdlog::error("Failed to retrieve Vulkan physical devices.");
        vkDestroyInstance(*instance, nullptr);
        return -1;
    }

    return 0;
}

int list_vulkan_devices() {
    VkInstance instance;
    std::vector<VkPhysicalDevice> physical_devices;
    int result = enumerate_vulkan_devices(&instance, physical_devices);
    if (result != 0) {
        return result;
    }

    uint32_t device_count = static_cast<uint32_t>(physical_devices.size());

    // List Vulkan device information
    for (uint32_t i = 0; i < device_count; i++) {
        VkPhysicalDevice device = physical_devices[i];
        VkPhysicalDeviceProperties device_properties;
        vkGetPhysicalDeviceProperties(device, &device_properties);

        // Print Vulkan device ID and name
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

        // Print device ID
        std::cout << "\tDevice ID: " << std::hex << std::showbase << device_properties.deviceID
                  << std::dec << std::endl;
    }

    // Clean up Vulkan instance
    vkDestroyInstance(instance, nullptr);
    return 0;
}

int get_vulkan_device_prop(uint32_t vk_device_index, VkPhysicalDeviceProperties *dev_props) {
    if (dev_props == nullptr) {
        spdlog::error("Invalid device properties pointer.");
        return -1;
    }

    VkInstance instance;
    std::vector<VkPhysicalDevice> devices;
    int result = enumerate_vulkan_devices(&instance, devices);
    if (result != 0) {
        return result;
    }

    uint32_t device_count = static_cast<uint32_t>(devices.size());

    // Check if the Vulkan device ID is valid
    if (vk_device_index >= device_count) {
        vkDestroyInstance(instance, nullptr);
        return -2;
    }

    // Get device properties for the specified Vulkan device ID
    vkGetPhysicalDeviceProperties(devices[vk_device_index], dev_props);

    // Clean up Vulkan instance
    vkDestroyInstance(instance, nullptr);

    return 0;
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
            ("log-level", PO_STR_VALUE<StringType>()->default_value(STR("info"), "info"),
                "Set verbosity level (trace, debug, info, warn, error, critical, none)")
            ("no-progress", po::bool_switch(&arguments.no_progress),
                "Do not display the progress bar")
            ("list-devices,l", "List the available Vulkan devices (GPUs)")

            // General Processing Options
            ("input,i", PO_STR_VALUE<StringType>(), "Input video file path")
            ("output,o", PO_STR_VALUE<StringType>(), "Output video file path")
            ("processor,p", PO_STR_VALUE<StringType>(&arguments.processor_type),
                "Processor to use (libplacebo, realesrgan, rife)")
            ("hwaccel,a", PO_STR_VALUE<StringType>(&arguments.hwaccel)->default_value(STR("none"),
                "none"), "Hardware acceleration method (decoding)")
            ("device,d", po::value<uint32_t>(&arguments.vk_device_index)->default_value(0),
                "Vulkan device index (GPU ID)")
            ("benchmark,b", po::bool_switch(&arguments.benchmark),
                "Discard processed frames and calculate average FPS; "
                "useful for detecting encoder bottlenecks")
        ;

        po::options_description encoder_opts("Encoder options");
        encoder_opts.add_options()
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

        po::options_description upscale_opts("Upscaling options");
        upscale_opts.add_options()
            ("width,w", po::value<int>(&arguments.width), "Output width")
            ("height,h", po::value<int>(&arguments.height), "Output height")
            ("scaling-factor,s", po::value<int>(&arguments.scaling_factor), "Scaling factor")
        ;

        po::options_description interp_opts("Frame interpolation options");
        interp_opts.add_options()
            ("frame-rate-mul,m",
                po::value<int>(&arguments.frm_rate_mul)->default_value(2),
                "Frame rate multiplier")
            ("scene-thresh,t", po::value<float>(&arguments.scn_det_thresh)->default_value(10.0f),
                "Scene detection threshold")
        ;

        po::options_description libplacebo_opts("libplacebo options");
        libplacebo_opts.add_options()
            ("libplacebo-shader", PO_STR_VALUE<StringType>(&arguments.libplacebo_shader_path),
                "Name/path of the GLSL shader file to use (built-in: anime4k-v4-a, anime4k-v4-a+a, "
                "anime4k-v4-b, anime4k-v4-b+b, anime4k-v4-c, anime4k-v4-c+a, anime4k-v4.1-gan)")
        ;

        po::options_description realesrgan_opts("RealESRGAN options");
        realesrgan_opts.add_options()
            ("realesrgan-model", PO_STR_VALUE<StringType>(&arguments.realesrgan_model_name),
                "Name of the RealESRGAN model to use (realesr-animevideov3, realesrgan-plus-anime, "
                "realesrgan-plus)")
        ;

        po::options_description rife_opts("RIFE options");
        rife_opts.add_options()
            ("rife-model", PO_STR_VALUE<StringType>(&arguments.rife_model_name),
                "Name of the RIFE model to use (rife, rife-HD, rife-UHD, rife-anime, rife-v2, "
                "rife-v2.3, rife-v2.4, rife-v3.0, rife-v3.1, rife-v4, rife-v4.6)")
            ("rife-uhd", po::bool_switch(&arguments.rife_uhd_mode),
                "Enable Ultra HD mode")
        ;
        // clang-format on

        // Combine all options
        all_opts.add(encoder_opts)
            .add(upscale_opts)
            .add(interp_opts)
            .add(libplacebo_opts)
            .add(realesrgan_opts)
            .add(rife_opts);

        // Positional arguments
        po::positional_options_description p;
        p.add("input", 1).add("output", 1).add("processor", 1);

        po::variables_map vm;
#ifdef _WIN32
        po::store(po::wcommand_line_parser(argc, argv).options(all_opts).positional(p).run(), vm);
#else
        po::store(po::command_line_parser(argc, argv).options(all_opts).positional(p).run(), vm);
#endif
        po::notify(vm);

        if (vm.count("help") || argc == 1) {
            std::cout
                << all_opts << std::endl
                << "Examples:" << std::endl
                << "  Upscale an anime video to 4K using libplacebo:" << std::endl
                << "    video2x -i input.mp4 -o output.mp4 -w 3840 -h 2160 \\" << std::endl
                << "      -p libplacebo --libplacebo-shader anime4k-v4-a+a" << std::endl
                << std::endl
                << "  Upscale a film by 4x using RealESRGAN with custom encoder options:"
                << std::endl
                << "    video2x -i input.mkv -o output.mkv -s 4 \\" << std::endl
                << "      -p realesrgan --realesrgan-model realesrgan-plus \\" << std::endl
                << "      -c libx264rgb -e crf=17 -e preset=veryslow -e tune=film" << std::endl
                << std::endl
                << "  Frame-interpolate a video using RIFE to 4x the original frame rate:"
                << std::endl
                << "    video2x -i input.mp4 -o output.mp4 -m 4 -p rife --rife-model rife-v4.6"
                << std::endl;
            return 0;
        }

        if (vm.count("version")) {
            std::cout << "Video2X version " << LIBVIDEO2X_VERSION_STRING << std::endl;
            return 0;
        }

        if (vm.count("list-devices")) {
            return list_vulkan_devices();
        }

        if (vm.count("log-level")) {
            std::optional<Video2xLogLevel> log_level =
                find_log_level_by_name(vm["log-level"].as<StringType>());
            if (!log_level.has_value()) {
                spdlog::critical("Invalid log level specified.");
                return 1;
            }
            arguments.log_level = log_level.value();
        }
        set_spdlog_level(arguments.log_level);

        // Print program banner
        spdlog::info("Video2X version {}", LIBVIDEO2X_VERSION_STRING);
        // spdlog::info("Copyright (C) 2018-2024 K4YT3X and contributors.");
        // spdlog::info("Licensed under GNU AGPL version 3.");

        // Assign positional arguments
        if (vm.count("input")) {
            arguments.in_fname = std::filesystem::path(vm["input"].as<StringType>());
            spdlog::info("Processing file: {}", arguments.in_fname.u8string());
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

        // Parse extra AVOptions
        if (vm.count("extra-encoder-option")) {
            for (const auto &opt : vm["extra-encoder-option"].as<std::vector<StringType>>()) {
                size_t eq_pos = opt.find('=');
                if (eq_pos != StringType::npos) {
                    StringType key = opt.substr(0, eq_pos);
                    StringType value = opt.substr(eq_pos + 1);
                    arguments.extra_encoder_opts.push_back(std::make_pair(key, value));
                } else {
                    spdlog::critical("Invalid extra AVOption format: {}", wstring_to_u8string(opt));
                    return 1;
                }
            }
        }

        if (vm.count("libplacebo-model")) {
            if (!is_valid_realesrgan_model(vm["realesrgan-model"].as<StringType>())) {
                spdlog::critical("Invalid model specified.");
                return 1;
            }
        }

        if (vm.count("rife-model")) {
            if (!is_valid_rife_model(vm["rife-model"].as<StringType>())) {
                spdlog::critical("Invalid RIFE model specified.");
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
    if (arguments.width < 0 || arguments.height < 0) {
        spdlog::critical("Invalid output resolution specified.");
        return 1;
    }
    if (arguments.scaling_factor < 0) {
        spdlog::critical("Invalid scaling factor specified.");
        return 1;
    }
    if (arguments.frm_rate_mul <= 1) {
        spdlog::critical("Invalid frame rate multiplier specified.");
        return 1;
    }
    if (arguments.scn_det_thresh < 0.0f || arguments.scn_det_thresh > 100.0f) {
        spdlog::critical("Invalid scene detection threshold specified.");
        return 1;
    }

    if (arguments.processor_type == STR("libplacebo")) {
        if (arguments.libplacebo_shader_path.empty() || arguments.width == 0 ||
            arguments.height == 0) {
            spdlog::critical("Shader name/path, width, and height are required for libplacebo.");
            return 1;
        }
    } else if (arguments.processor_type == STR("realesrgan")) {
        if (arguments.scaling_factor != 2 && arguments.scaling_factor != 3 &&
            arguments.scaling_factor != 4) {
            spdlog::critical("Scaling factor must be 2, 3, or 4 for RealESRGAN.");
            return 1;
        }
    } else if (arguments.processor_type != STR("rife")) {
        spdlog::critical(
            "Invalid processor specified. Must be 'libplacebo', 'realesrgan', or 'rife'."
        );
        return 1;
    }

    // Validate GPU ID
    VkPhysicalDeviceProperties dev_props;
    int get_vulkan_dev_ret = get_vulkan_device_prop(arguments.vk_device_index, &dev_props);
    if (get_vulkan_dev_ret != 0) {
        if (get_vulkan_dev_ret == -2) {
            spdlog::critical("Invalid Vulkan device ID specified.");
            return 1;
        } else {
            spdlog::warn("Unable to validate Vulkan device ID.");
            return 1;
        }
    } else {
        // Warn if the selected device is a CPU
        spdlog::info("Using Vulkan device: {} ({:#x})", dev_props.deviceName, dev_props.deviceID);
        if (dev_props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
            spdlog::warn("The selected Vulkan device is a CPU device.");
        }
    }

    // Validate bitrate
    if (arguments.bit_rate < 0) {
        spdlog::critical("Invalid bitrate specified.");
        return 1;
    }

    // Parse codec to AVCodec
    const AVCodec *codec =
        avcodec_find_encoder_by_name(wstring_to_u8string(arguments.codec).c_str());
    if (!codec) {
        spdlog::critical("Codec '{}' not found.", wstring_to_u8string(arguments.codec));
        return 1;
    }

    // Parse pixel format to AVPixelFormat
    AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;
    if (!arguments.pix_fmt.empty()) {
        pix_fmt = av_get_pix_fmt(wstring_to_u8string(arguments.pix_fmt).c_str());
        if (pix_fmt == AV_PIX_FMT_NONE) {
            spdlog::critical("Invalid pixel format '{}'.", wstring_to_u8string(arguments.pix_fmt));
            return 1;
        }
    }

    // Setup filter configurations based on the parsed arguments
    ProcessorConfig proc_cfg;
    proc_cfg.width = arguments.width;
    proc_cfg.height = arguments.height;
    proc_cfg.scaling_factor = arguments.scaling_factor;
    proc_cfg.frm_rate_mul = arguments.frm_rate_mul;
    proc_cfg.scn_det_thresh = arguments.scn_det_thresh;

    if (arguments.processor_type == STR("libplacebo")) {
        proc_cfg.processor_type = ProcessorType::Libplacebo;
        LibplaceboConfig libplacebo_config;
        libplacebo_config.shader_path = arguments.libplacebo_shader_path;
        proc_cfg.config = libplacebo_config;
    } else if (arguments.processor_type == STR("realesrgan")) {
        proc_cfg.processor_type = ProcessorType::RealESRGAN;
        RealESRGANConfig realesrgan_config;
        realesrgan_config.tta_mode = false;
        realesrgan_config.model_name = arguments.realesrgan_model_name;
        proc_cfg.config = realesrgan_config;
    } else if (arguments.processor_type == STR("rife")) {
        proc_cfg.processor_type = ProcessorType::RIFE;
        RIFEConfig rife_config;
        rife_config.tta_mode = false;
        rife_config.tta_temporal_mode = false;
        rife_config.uhd_mode = arguments.rife_uhd_mode;
        rife_config.num_threads = 0;
        rife_config.model_name = arguments.rife_model_name;
        proc_cfg.config = rife_config;
    }

    // Setup encoder configuration
    EncoderConfig enc_cfg;
    enc_cfg.codec = codec->id;
    enc_cfg.copy_streams = !arguments.no_copy_streams;
    enc_cfg.width = 0;
    enc_cfg.height = 0;
    enc_cfg.pix_fmt = pix_fmt;
    enc_cfg.bit_rate = arguments.bit_rate;
    enc_cfg.rc_buffer_size = arguments.rc_buffer_size;
    enc_cfg.rc_max_rate = arguments.rc_max_rate;
    enc_cfg.rc_min_rate = arguments.rc_min_rate;
    enc_cfg.qmin = arguments.qmin;
    enc_cfg.qmax = arguments.qmax;
    enc_cfg.gop_size = arguments.gop_size;
    enc_cfg.max_b_frames = arguments.max_b_frames;
    enc_cfg.keyint_min = arguments.keyint_min;
    enc_cfg.refs = arguments.refs;
    enc_cfg.thread_count = arguments.thread_count;
    enc_cfg.delay = arguments.delay;
    enc_cfg.extra_opts = arguments.extra_encoder_opts;

    // Setup hardware configuration
    HardwareConfig hw_cfg;
    hw_cfg.hw_device_type = AV_HWDEVICE_TYPE_NONE;
    hw_cfg.vk_device_index = arguments.vk_device_index;

    // Parse hardware acceleration method
    if (arguments.hwaccel != STR("none")) {
        hw_cfg.hw_device_type =
            av_hwdevice_find_type_by_name(wstring_to_u8string(arguments.hwaccel).c_str());
        if (hw_cfg.hw_device_type == AV_HWDEVICE_TYPE_NONE) {
            spdlog::critical(
                "Invalid hardware device type '{}'.", wstring_to_u8string(arguments.hwaccel)
            );
            return 1;
        }
    }

    // Create video processor object
    VideoProcessor video_processor =
        VideoProcessor(hw_cfg, proc_cfg, enc_cfg, arguments.log_level, arguments.benchmark);

    // Register a newline-safe log callback for FFmpeg
    av_log_set_callback(newline_safe_ffmpeg_log_callback);

    // Create a thread for video processing
    int proc_ret = 0;
    std::atomic<bool> completed = false;  // Use atomic for thread-safe updates
    std::thread processing_thread([&]() {
        proc_ret = video_processor.process(arguments.in_fname, arguments.out_fname);
        completed.store(true, std::memory_order_relaxed);
    });
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
        if (completed.load()) {
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
                if (video_processor.is_paused()) {
                    video_processor.resume();
                } else {
                    video_processor.pause();
                }
                if (video_processor.is_paused()) {
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
                video_processor.abort();
                newline_required = false;
            }
            break;
        }

        // Display progress
        if (!arguments.no_progress) {
            int64_t processed_frames, total_frames;
            bool paused;
            {
                processed_frames = video_processor.get_processed_frames();
                total_frames = video_processor.get_total_frames();
                paused = video_processor.is_paused();
            }
            if (!paused && (total_frames > 0 || processed_frames > 0)) {
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
    if (video_processor.is_aborted()) {
        spdlog::warn("Video processing aborted");
        return 2;
    } else if (proc_ret != 0) {
        spdlog::critical("Video processing failed with error code {}", proc_ret);
        return 1;
    } else {
        spdlog::info("Video processed successfully");
    }

    // Calculate statistics
    int64_t processed_frames = video_processor.get_processed_frames();
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
