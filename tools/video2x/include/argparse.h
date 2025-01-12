#pragma once

#include <libvideo2x/libvideo2x.h>
#include <filesystem>

// Structure to hold parsed arguments
struct Arguments {
    bool no_progress = false;
    std::filesystem::path in_fname;
    std::filesystem::path out_fname;
    uint32_t vk_device_index = 0;
    AVHWDeviceType hw_device_type = AV_HWDEVICE_TYPE_NONE;
    bool benchmark = false;
};

[[nodiscard]] int parse_args(
    int argc,
#ifdef _WIN32
    wchar_t* argv[],
#else
    char* argv[],
#endif
    Arguments& arguments,
    video2x::processors::ProcessorConfig& proc_cfg,
    video2x::encoder::EncoderConfig& enc_cfg
);
