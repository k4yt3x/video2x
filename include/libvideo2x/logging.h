#ifndef LOGGING_H
#define LOGGING_H

#include <optional>

#include "fsutils.h"

enum class Libvideo2xLogLevel {
    Unknown,
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Critical,
    Off
};

void set_log_level(Libvideo2xLogLevel log_level);

std::optional<Libvideo2xLogLevel> find_log_level_by_name(const StringType &log_level_name);

#endif  // LOGGING_H
