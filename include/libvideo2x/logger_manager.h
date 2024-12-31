#pragma once

#include <memory>
#include <string>
#include <vector>

#include <spdlog/logger.h>
#include <spdlog/sinks/sink.h>

#include "libvideo2x_export.h"

namespace video2x {
namespace logger_manager {

class LIBVIDEO2X_API LoggerManager {
   public:
    LoggerManager(const LoggerManager&) = delete;
    LoggerManager& operator=(const LoggerManager&) = delete;

    static LoggerManager& instance();

    std::shared_ptr<spdlog::logger> logger();

    bool reconfigure_logger(
        const std::string& logger_name,
        const std::vector<spdlog::sink_ptr>& sinks,
        const std::string& pattern = "%+"
    );

    bool set_log_level(const std::string& level_str);

    void hook_ffmpeg_logging();
    void unhook_ffmpeg_logging();

   private:
    LoggerManager();

    std::shared_ptr<spdlog::logger> logger_ = nullptr;
};

}  // namespace logger_manager

// Convenience function to get the logger instance
inline std::shared_ptr<spdlog::logger> logger() {
    return logger_manager::LoggerManager::instance().logger();
}

}  // namespace video2x
