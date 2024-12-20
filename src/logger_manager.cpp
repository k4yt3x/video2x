#include "logger_manager.h"

extern "C" {
#include <libavutil/log.h>
}

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

static spdlog::level::level_enum ffmpeg_level_to_spdlog(int av_level) {
    if (av_level <= AV_LOG_PANIC) {
        return spdlog::level::critical;
    } else if (av_level <= AV_LOG_ERROR) {
        return spdlog::level::err;
    } else if (av_level <= AV_LOG_WARNING) {
        return spdlog::level::warn;
    } else if (av_level <= AV_LOG_INFO) {
        return spdlog::level::info;
    } else if (av_level <= AV_LOG_VERBOSE) {
        return spdlog::level::debug;
    } else if (av_level == AV_LOG_DEBUG) {
        return spdlog::level::debug;
    } else {
        // AV_LOG_TRACE or beyond (if supported by FFmpeg)
        return spdlog::level::trace;
    }
}

static void ffmpeg_log_callback(void *, int av_level, const char *fmt, va_list vargs) {
    // Format the message into a buffer
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, vargs);

    // Trim trailing newlines
    std::string message = buffer;
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
        message.pop_back();
    }

    // Forward FFmpeg log message to the logger instance
    video2x::logger()->log(ffmpeg_level_to_spdlog(av_level), message);
}

namespace video2x {
namespace logger_manager {

LoggerManager::LoggerManager() {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern("%+");
    logger_ = std::make_shared<spdlog::logger>("video2x", console_sink);
    spdlog::register_logger(logger_);
    logger_->set_level(spdlog::level::info);
}

LoggerManager &LoggerManager::instance() {
    static LoggerManager instance;
    return instance;
}

std::shared_ptr<spdlog::logger> LoggerManager::logger() {
    return logger_;
}

void LoggerManager::reconfigure_logger(
    const std::string &logger_name,
    const std::vector<spdlog::sink_ptr> &sinks,
    const std::string &pattern
) {
    if (!sinks.empty()) {
        // If a logger with the same name exists, remove it first
        auto old_logger = spdlog::get(logger_name);
        if (old_logger) {
            spdlog::drop(logger_name);
        }

        // Create a new logger with the given name, sinks, and pattern
        auto new_logger = std::make_shared<spdlog::logger>(logger_name, sinks.begin(), sinks.end());
        new_logger->set_pattern(pattern);

        // Maintain the log level from the previous logger
        if (logger_) {
            new_logger->set_level(logger_->level());
        }

        // Replace the internal logger_ member and register the new one
        logger_ = new_logger;
        spdlog::register_logger(logger_);
    }
}

bool LoggerManager::set_log_level(const std::string &level_str) {
    spdlog::level::level_enum log_level = spdlog::level::from_str(level_str);
    if (log_level == spdlog::level::off && level_str != "off") {
        // Invalid level_str
        return false;
    }
    logger_->set_level(log_level);
    return true;
}

void LoggerManager::hook_ffmpeg_logging() {
    av_log_set_callback(ffmpeg_log_callback);
}

void LoggerManager::unhook_ffmpeg_logging() {
    av_log_set_callback(nullptr);
}

}  // namespace logger_manager
}  // namespace video2x
