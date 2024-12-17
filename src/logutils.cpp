#include "logutils.h"

extern "C" {
#include <libavutil/avutil.h>
}

#include <spdlog/spdlog.h>

namespace video2x {
namespace logutils {

void set_log_level(Video2xLogLevel log_level) {
    switch (log_level) {
        case Video2xLogLevel::Trace:
            av_log_set_level(AV_LOG_TRACE);
            spdlog::set_level(spdlog::level::trace);
            break;
        case Video2xLogLevel::Debug:
            av_log_set_level(AV_LOG_DEBUG);
            spdlog::set_level(spdlog::level::debug);
            break;
        case Video2xLogLevel::Info:
            av_log_set_level(AV_LOG_INFO);
            spdlog::set_level(spdlog::level::info);
            break;
        case Video2xLogLevel::Warning:
            av_log_set_level(AV_LOG_WARNING);
            spdlog::set_level(spdlog::level::warn);
            break;
        case Video2xLogLevel::Error:
            av_log_set_level(AV_LOG_ERROR);
            spdlog::set_level(spdlog::level::err);
            break;
        case Video2xLogLevel::Critical:
            av_log_set_level(AV_LOG_FATAL);
            spdlog::set_level(spdlog::level::critical);
            break;
        case Video2xLogLevel::Off:
            av_log_set_level(AV_LOG_QUIET);
            spdlog::set_level(spdlog::level::off);
            break;
        default:
            av_log_set_level(AV_LOG_INFO);
            spdlog::set_level(spdlog::level::info);
            break;
    }
}

}  // namespace logutils
}  // namespace video2x
