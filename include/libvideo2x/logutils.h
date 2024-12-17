#pragma once

#include <optional>

#include "fsutils.h"

namespace video2x {
namespace logutils {

enum class Video2xLogLevel {
    Unknown,
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Critical,
    Off
};

void set_log_level(Video2xLogLevel log_level);

std::optional<Video2xLogLevel> find_log_level_by_name(
    const fsutils::StringType &log_level_name
);

}  // namespace logutils
}  // namespace video2x
