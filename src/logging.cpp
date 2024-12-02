#include "logging.h"

#include <algorithm>

extern "C" {
#include <libavutil/avutil.h>
}

#include <spdlog/spdlog.h>

void set_log_level(Libvideo2xLogLevel log_level) {
    switch (log_level) {
        case Libvideo2xLogLevel::Trace:
            av_log_set_level(AV_LOG_TRACE);
            spdlog::set_level(spdlog::level::trace);
            break;
        case Libvideo2xLogLevel::Debug:
            av_log_set_level(AV_LOG_DEBUG);
            spdlog::set_level(spdlog::level::debug);
            break;
        case Libvideo2xLogLevel::Info:
            av_log_set_level(AV_LOG_INFO);
            spdlog::set_level(spdlog::level::info);
            break;
        case Libvideo2xLogLevel::Warning:
            av_log_set_level(AV_LOG_WARNING);
            spdlog::set_level(spdlog::level::warn);
            break;
        case Libvideo2xLogLevel::Error:
            av_log_set_level(AV_LOG_ERROR);
            spdlog::set_level(spdlog::level::err);
            break;
        case Libvideo2xLogLevel::Critical:
            av_log_set_level(AV_LOG_FATAL);
            spdlog::set_level(spdlog::level::critical);
            break;
        case Libvideo2xLogLevel::Off:
            av_log_set_level(AV_LOG_QUIET);
            spdlog::set_level(spdlog::level::off);
            break;
        default:
            av_log_set_level(AV_LOG_INFO);
            spdlog::set_level(spdlog::level::info);
            break;
    }
}

std::optional<Libvideo2xLogLevel> find_log_level_by_name(const StringType &log_level_name) {
    // Static map to store the mapping
    static const std::unordered_map<StringType, Libvideo2xLogLevel> LogLevelMap = {
        {STR("trace"), Libvideo2xLogLevel::Trace},
        {STR("debug"), Libvideo2xLogLevel::Debug},
        {STR("info"), Libvideo2xLogLevel::Info},
        {STR("warning"), Libvideo2xLogLevel::Warning},
        {STR("warn"), Libvideo2xLogLevel::Warning},
        {STR("error"), Libvideo2xLogLevel::Error},
        {STR("critical"), Libvideo2xLogLevel::Critical},
        {STR("off"), Libvideo2xLogLevel::Off},
        {STR("none"), Libvideo2xLogLevel::Off}
    };

    // Normalize the input to lowercase
    StringType normalized_name = log_level_name;
    std::transform(
        normalized_name.begin(), normalized_name.end(), normalized_name.begin(), ::tolower
    );

    // Lookup the log level in the map
    auto it = LogLevelMap.find(normalized_name);
    if (it != LogLevelMap.end()) {
        return it->second;
    }

    return std::nullopt;
}
