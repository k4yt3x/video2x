#include "logging.h"

#include <algorithm>
#include <unordered_map>

extern "C" {
#include <libavutil/log.h>
}

std::atomic<bool> newline_required = false;

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
    if (level <= av_log_get_level() && newline_required.load()) {
        putchar('\n');
        newline_required.store(false);
    }
    av_log_default_callback(ptr, level, fmt, vl);
}
