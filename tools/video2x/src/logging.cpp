#include "logging.h"

#include <algorithm>
#include <unordered_map>

extern "C" {
#include <libavutil/log.h>
}

std::atomic<bool> newline_required = false;

void set_spdlog_level(video2x::logutils::Video2xLogLevel log_level) {
    switch (log_level) {
        case video2x::logutils::Video2xLogLevel::Trace:
            spdlog::set_level(spdlog::level::trace);
            break;
        case video2x::logutils::Video2xLogLevel::Debug:
            spdlog::set_level(spdlog::level::debug);
            break;
        case video2x::logutils::Video2xLogLevel::Info:
            spdlog::set_level(spdlog::level::info);
            break;
        case video2x::logutils::Video2xLogLevel::Warning:
            spdlog::set_level(spdlog::level::warn);
            break;
        case video2x::logutils::Video2xLogLevel::Error:
            spdlog::set_level(spdlog::level::err);
            break;
        case video2x::logutils::Video2xLogLevel::Critical:
            spdlog::set_level(spdlog::level::critical);
            break;
        case video2x::logutils::Video2xLogLevel::Off:
            spdlog::set_level(spdlog::level::off);
            break;
        default:
            spdlog::set_level(spdlog::level::info);
            break;
    }
}

std::optional<video2x::logutils::Video2xLogLevel> find_log_level_by_name(
    const video2x::fsutils::StringType &log_level_name
) {
    // Static map to store the mapping
    static const std::
        unordered_map<video2x::fsutils::StringType, video2x::logutils::Video2xLogLevel>
            log_level_map = {
                {STR("trace"), video2x::logutils::Video2xLogLevel::Trace},
                {STR("debug"), video2x::logutils::Video2xLogLevel::Debug},
                {STR("info"), video2x::logutils::Video2xLogLevel::Info},
                {STR("warning"), video2x::logutils::Video2xLogLevel::Warning},
                {STR("warn"), video2x::logutils::Video2xLogLevel::Warning},
                {STR("error"), video2x::logutils::Video2xLogLevel::Error},
                {STR("critical"), video2x::logutils::Video2xLogLevel::Critical},
                {STR("off"), video2x::logutils::Video2xLogLevel::Off},
                {STR("none"), video2x::logutils::Video2xLogLevel::Off}
            };

    // Normalize the input to lowercase
    video2x::fsutils::StringType normalized_name = log_level_name;
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
