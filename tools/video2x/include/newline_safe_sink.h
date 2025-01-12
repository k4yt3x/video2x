#pragma once

#include <atomic>

#include <spdlog/sinks/ansicolor_sink.h>

class newline_safe_sink : public spdlog::sinks::ansicolor_stdout_sink_mt {
   public:
    newline_safe_sink() = default;
    ~newline_safe_sink() = default;

    newline_safe_sink(const newline_safe_sink&) = delete;
    newline_safe_sink& operator=(const newline_safe_sink&) = delete;

    void log(const spdlog::details::log_msg& msg);

    void set_needs_newline(bool needs_newline) { needs_newline_.store(needs_newline); };
    bool get_needs_newline() { return needs_newline_.load(); };

   private:
    std::atomic<bool> needs_newline_ = false;
};
