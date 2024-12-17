#include "argparse.h"

#include <iostream>

#if _WIN32
#include <Windows.h>
#include <cwchar>
#endif

#include <libvideo2x/version.h>
#include <spdlog/spdlog.h>
#include <vulkan_utils.h>
#include <boost/program_options.hpp>

#include "logging.h"
#include "validators.h"

#ifdef _WIN32
#define BOOST_PROGRAM_OPTIONS_WCHAR_T
#define PO_STR_VALUE po::wvalue
#else
#define PO_STR_VALUE po::value
#endif

namespace po = boost::program_options;

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

int parse_args(
    int argc,
#ifdef _WIN32
    wchar_t *argv[],
#else
    char *argv[],
#endif
    Arguments &arguments,
    video2x::processors::ProcessorConfig &proc_cfg,
    video2x::encoder::EncoderConfig &enc_cfg
) {
    try {
        // clang-format off
        po::options_description all_opts("General options");
        all_opts.add_options()
            ("help", "Display this help page")
            ("version,V", "Print program version and exit")
            ("log-level", PO_STR_VALUE<video2x::fsutils::StringType>()
                ->default_value(STR("info"), "info"),
                "Set verbosity level (trace, debug, info, warn, error, critical, none)")
            ("no-progress", po::bool_switch(&arguments.no_progress),
                "Do not display the progress bar")
            ("list-devices,l", "List the available Vulkan devices (GPUs)")

            // General Processing Options
            ("input,i", PO_STR_VALUE<video2x::fsutils::StringType>(), "Input video file path")
            ("output,o", PO_STR_VALUE<video2x::fsutils::StringType>(), "Output video file path")
            ("processor,p", PO_STR_VALUE<video2x::fsutils::StringType>(),
                "Processor to use (libplacebo, realesrgan, rife)")
            ("hwaccel,a", PO_STR_VALUE<video2x::fsutils::StringType>()
                ->default_value(STR("none"), "none"), "Hardware acceleration method (decoding)")
            ("device,d", po::value<uint32_t>(&arguments.vk_device_index)->default_value(0),
                "Vulkan device index (GPU ID)")
            ("benchmark,b", po::bool_switch(&arguments.benchmark),
                "Discard processed frames and calculate average FPS; "
                "useful for detecting encoder bottlenecks")
        ;

        po::options_description encoder_opts("Encoder options");
        encoder_opts.add_options()
            ("codec,c", PO_STR_VALUE<video2x::fsutils::StringType>()
                ->default_value(STR("libx264"), "libx264"), "Output codec")
            ("no-copy-streams", "Do not copy audio and subtitle streams")
            ("pix-fmt", PO_STR_VALUE<video2x::fsutils::StringType>(), "Output pixel format")
            ("bit-rate", po::value<int64_t>(&enc_cfg.bit_rate)->default_value(0),
                "Bitrate in bits per second")
            ("rc-buffer-size", po::value<int>(&enc_cfg.rc_buffer_size)->default_value(0),
                "Rate control buffer size in bits")
            ("rc-min-rate", po::value<int>(&enc_cfg.rc_min_rate)->default_value(0),
                "Minimum rate control")
            ("rc-max-rate", po::value<int>(&enc_cfg.rc_max_rate)->default_value(0),
                "Maximum rate control")
            ("qmin", po::value<int>(&enc_cfg.qmin)->default_value(-1), "Minimum quantizer")
            ("qmax", po::value<int>(&enc_cfg.qmax)->default_value(-1), "Maximum quantizer")
            ("gop-size", po::value<int>(&enc_cfg.gop_size)->default_value(-1),
                "Group of pictures structure size")
            ("max-b-frames", po::value<int>(&enc_cfg.max_b_frames)->default_value(-1),
                "Maximum number of B-frames")
            ("keyint-min", po::value<int>(&enc_cfg.keyint_min)->default_value(-1),
                "Minimum interval between keyframes")
            ("refs", po::value<int>(&enc_cfg.refs)->default_value(-1),
                "Number of reference frames")
            ("thread-count", po::value<int>(&enc_cfg.thread_count)->default_value(0),
                "Number of threads for encoding")
            ("delay", po::value<int>(&enc_cfg.delay)->default_value(0),
                "Delay in milliseconds for encoder")

            // Extra encoder options (key-value pairs)
            ("extra-encoder-option,e", PO_STR_VALUE<std::vector<video2x::fsutils::StringType>>()
                ->multitoken(), "Additional AVOption(s) for the encoder (format: -e key=value)")
        ;

        po::options_description upscale_opts("Upscaling options");
        upscale_opts.add_options()
            ("width,w", po::value<int>(&proc_cfg.width)
                ->notifier([](int v) { validate_greater_equal_one(v, "width"); }), "Output width")
            ("height,h", po::value<int>(&proc_cfg.height)
                ->notifier([](int v) { validate_greater_equal_one(v, "height"); }), "Output height")
            ("scaling-factor,s", po::value<int>(&proc_cfg.scaling_factor)
                ->notifier([](int v) { validate_min(v, "scaling-factor", 2); }), "Scaling factor")
        ;

        po::options_description interp_opts("Frame interpolation options");
        interp_opts.add_options()
            ("frame-rate-mul,m", po::value<int>(&proc_cfg.frm_rate_mul)
                ->notifier([](int v) { validate_min(v, "frame-rate-mul", 2); }),
                "Frame rate multiplier")
            ("scene-thresh,t", po::value<float>(&proc_cfg.scn_det_thresh)->default_value(100.0f)
                ->notifier([](float v) { validate_range<float>(v, "scene-thresh", 0.0, 100.0); }),
                "Scene detection threshold (20 means 20% diff between frames is a scene change)")
        ;

        po::options_description libplacebo_opts("libplacebo options");
        libplacebo_opts.add_options()
            ("libplacebo-shader", PO_STR_VALUE<video2x::fsutils::StringType>()
                ->default_value(STR("anime4k-v4-a"), "anime4k-v4-a")
                ->notifier(validate_anime4k_shader_name),
                "Name/path of the GLSL shader file to use (built-in: anime4k-v4-a, anime4k-v4-a+a, "
                "anime4k-v4-b, anime4k-v4-b+b, anime4k-v4-c, anime4k-v4-c+a, anime4k-v4.1-gan)")
        ;

        po::options_description realesrgan_opts("RealESRGAN options");
        realesrgan_opts.add_options()
            ("realesrgan-model", PO_STR_VALUE<video2x::fsutils::StringType>()
                ->default_value(STR("realesr-animevideov3"), "realesr-animevideov3")
                ->notifier(validate_realesrgan_model_name),
                "Name of the RealESRGAN model to use (realesr-animevideov3, realesrgan-plus-anime, "
                "realesrgan-plus)")
        ;

        po::options_description rife_opts("RIFE options");
        rife_opts.add_options()
            ("rife-model", PO_STR_VALUE<video2x::fsutils::StringType>()
                ->default_value(STR("rife-v4.6"), "rife-v4.6")->notifier(validate_rife_model_name),
                "Name of the RIFE model to use (rife, rife-HD, rife-UHD, rife-anime, rife-v2, "
                "rife-v2.3, rife-v2.4, rife-v3.0, rife-v3.1, rife-v4, rife-v4.6)")
            ("rife-uhd", "Enable Ultra HD mode")
        ;
        // clang-format on

        // Combine all options
        all_opts.add(encoder_opts)
            .add(upscale_opts)
            .add(interp_opts)
            .add(libplacebo_opts)
            .add(realesrgan_opts)
            .add(rife_opts);

        po::variables_map vm;
#ifdef _WIN32
        po::store(po::wcommand_line_parser(argc, argv).options(all_opts).run(), vm);
#else
        po::store(po::command_line_parser(argc, argv).options(all_opts).run(), vm);
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
            return 1;
        }

        if (vm.count("version")) {
            std::cout << "Video2X version " << LIBVIDEO2X_VERSION_STRING << std::endl;
            return 1;
        }

        if (vm.count("list-devices")) {
            if (list_vulkan_devices()) {
                return -1;
            }
            return 1;
        }

        if (vm.count("log-level")) {
            std::optional<video2x::logutils::Video2xLogLevel> log_level =
                find_log_level_by_name(vm["log-level"].as<video2x::fsutils::StringType>());
            if (!log_level.has_value()) {
                spdlog::critical("Invalid log level specified.");
                return -1;
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
            arguments.in_fname =
                std::filesystem::path(vm["input"].as<video2x::fsutils::StringType>());
            spdlog::info("Processing file: {}", arguments.in_fname.u8string());
        } else {
            spdlog::critical("Input file path is required.");
            return -1;
        }

        if (vm.count("output")) {
            arguments.out_fname =
                std::filesystem::path(vm["output"].as<video2x::fsutils::StringType>());
        } else if (!arguments.benchmark) {
            spdlog::critical("Output file path is required.");
            return -1;
        }

        // Parse processor type
        if (vm.count("processor")) {
            video2x::fsutils::StringType processor_type_str =
                vm["processor"].as<video2x::fsutils::StringType>();
            if (processor_type_str == STR("libplacebo")) {
                proc_cfg.processor_type = video2x::processors::ProcessorType::Libplacebo;
            } else if (processor_type_str == STR("realesrgan")) {
                proc_cfg.processor_type = video2x::processors::ProcessorType::RealESRGAN;
            } else if (processor_type_str == STR("rife")) {
                proc_cfg.processor_type = video2x::processors::ProcessorType::RIFE;
            } else {
                spdlog::critical(
                    "Invalid processor specified. Must be 'libplacebo', 'realesrgan', or 'rife'."
                );
                return -1;
            }
        } else {
            spdlog::critical("Processor type is required.");
            return -1;
        }

        // Parse hardware acceleration method
        arguments.hw_device_type = AV_HWDEVICE_TYPE_NONE;
        if (vm.count("hwaccel")) {
            video2x::fsutils::StringType hwaccel_str =
                vm["hwaccel"].as<video2x::fsutils::StringType>();
            if (hwaccel_str != STR("none")) {
                arguments.hw_device_type =
                    av_hwdevice_find_type_by_name(wstring_to_u8string(hwaccel_str).c_str());
                if (arguments.hw_device_type == AV_HWDEVICE_TYPE_NONE) {
                    spdlog::critical(
                        "Invalid hardware device type '{}'.", wstring_to_u8string(hwaccel_str)
                    );
                    return -1;
                }
            }
        }

        // Parse codec to AVCodec
        enc_cfg.codec = AV_CODEC_ID_H264;
        if (vm.count("codec")) {
            video2x::fsutils::StringType codec_str = vm["codec"].as<video2x::fsutils::StringType>();
            const AVCodec *codec =
                avcodec_find_encoder_by_name(wstring_to_u8string(codec_str).c_str());
            if (codec == nullptr) {
                spdlog::critical("Codec '{}' not found.", wstring_to_u8string(codec_str));
                return -1;
            }
            enc_cfg.codec = codec->id;
        }

        // Parse copy streams flag
        enc_cfg.copy_streams = vm.count("no-copy-streams") == 0;

        // Parse pixel format to AVPixelFormat
        enc_cfg.pix_fmt = AV_PIX_FMT_NONE;
        if (vm.count("pix-fmt")) {
            video2x::fsutils::StringType pix_fmt_str =
                vm["pix-fmt"].as<video2x::fsutils::StringType>();
            if (!pix_fmt_str.empty()) {
                enc_cfg.pix_fmt = av_get_pix_fmt(wstring_to_u8string(pix_fmt_str).c_str());
                if (enc_cfg.pix_fmt == AV_PIX_FMT_NONE) {
                    spdlog::critical(
                        "Invalid pixel format '{}'.", wstring_to_u8string(pix_fmt_str)
                    );
                    return -1;
                }
            }
        }

        // Parse extra AVOptions
        if (vm.count("extra-encoder-option")) {
            for (const auto &opt :
                 vm["extra-encoder-option"].as<std::vector<video2x::fsutils::StringType>>()) {
                size_t eq_pos = opt.find('=');
                if (eq_pos != video2x::fsutils::StringType::npos) {
                    video2x::fsutils::StringType key = opt.substr(0, eq_pos);
                    video2x::fsutils::StringType value = opt.substr(eq_pos + 1);
                    enc_cfg.extra_opts.push_back(std::make_pair(key, value));
                } else {
                    spdlog::critical("Invalid extra AVOption format: {}", wstring_to_u8string(opt));
                    return -1;
                }
            }
        }

        // Parse processor-specific configurations
        switch (proc_cfg.processor_type) {
            case video2x::processors::ProcessorType::Libplacebo: {
                if (!vm.count("libplacebo-shader")) {
                    spdlog::critical("Shader name/path must be set for libplacebo.");
                    return -1;
                }
                if (proc_cfg.width <= 0 || proc_cfg.height <= 0) {
                    spdlog::critical("Output width and height must be set for libplacebo.");
                    return -1;
                }

                proc_cfg.processor_type = video2x::processors::ProcessorType::Libplacebo;
                video2x::processors::LibplaceboConfig libplacebo_config;
                libplacebo_config.shader_path =
                    vm["libplacebo-shader"].as<video2x::fsutils::StringType>();
                proc_cfg.config = libplacebo_config;
                break;
            }
            case video2x::processors::ProcessorType::RealESRGAN: {
                if (!vm.count("realesrgan-model")) {
                    spdlog::critical("RealESRGAN model name must be set for RealESRGAN.");
                    return -1;
                }
                if (proc_cfg.scaling_factor != 2 && proc_cfg.scaling_factor != 3 &&
                    proc_cfg.scaling_factor != 4) {
                    spdlog::critical("Scaling factor must be set to 2, 3, or 4 for RealESRGAN.");
                    return -1;
                }

                proc_cfg.processor_type = video2x::processors::ProcessorType::RealESRGAN;
                video2x::processors::RealESRGANConfig realesrgan_config;
                realesrgan_config.tta_mode = false;
                realesrgan_config.model_name =
                    vm["realesrgan-model"].as<video2x::fsutils::StringType>();
                proc_cfg.config = realesrgan_config;
                break;
            }
            case video2x::processors::ProcessorType::RIFE: {
                if (!vm.count("rife-model")) {
                    spdlog::critical("RIFE model name must be set for RIFE.");
                    return -1;
                }
                if (proc_cfg.frm_rate_mul < 2) {
                    spdlog::critical("Frame rate multiplier must be set to at least 2 for RIFE.");
                    return -1;
                }

                proc_cfg.processor_type = video2x::processors::ProcessorType::RIFE;
                video2x::processors::RIFEConfig rife_config;
                rife_config.tta_mode = false;
                rife_config.tta_temporal_mode = false;
                rife_config.uhd_mode = vm.count("rife-uhd") > 0;
                rife_config.num_threads = 0;
                rife_config.model_name = vm["rife-model"].as<video2x::fsutils::StringType>();
                proc_cfg.config = rife_config;
                break;
            }
            default:
                spdlog::critical("Invalid processor type.");
                return -1;
        }
    } catch (const po::error &e) {
        spdlog::critical("Error parsing arguments: {}", e.what());
        return -1;
    } catch (const std::exception &e) {
        spdlog::critical("Unexpected exception caught while parsing options: {}", e.what());
        return -1;
    }

    // Validate Vulkan device ID
    VkPhysicalDeviceProperties dev_props;
    int get_vulkan_dev_ret = get_vulkan_device_prop(arguments.vk_device_index, &dev_props);
    if (get_vulkan_dev_ret != 0) {
        if (get_vulkan_dev_ret == -2) {
            spdlog::critical("Invalid Vulkan device ID specified.");
            return -1;
        } else {
            spdlog::warn("Unable to validate Vulkan device ID.");
            return -1;
        }
    } else {
        // Warn if the selected device is a CPU
        spdlog::info("Using Vulkan device: {} ({:#x})", dev_props.deviceName, dev_props.deviceID);
        if (dev_props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
            spdlog::warn("The selected Vulkan device is a CPU device.");
        }
    }
    return 0;
}
