#pragma once

#include <atomic>
#include <optional>

#include <libvideo2x/libvideo2x.h>
#include <spdlog/spdlog.h>

extern std::atomic<bool> newline_required;

void set_spdlog_level(Video2xLogLevel log_level);

std::optional<Video2xLogLevel> find_log_level_by_name(const StringType &log_level_name);

void newline_safe_ffmpeg_log_callback(void *ptr, int level, const char *fmt, va_list vl);
