#ifndef LOGGING_H
#define LOGGING_H

#include <optional>

#include "fsutils.h"

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

std::optional<Video2xLogLevel> find_log_level_by_name(const StringType &log_level_name);

#endif  // LOGGING_H
