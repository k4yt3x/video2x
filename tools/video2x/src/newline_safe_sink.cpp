#include "newline_safe_sink.h"

void newline_safe_sink::log(const spdlog::details::log_msg& msg) {
    if (needs_newline_.exchange(false)) {
        std::fputs("\n", stdout);
    }

    spdlog::sinks::ansicolor_stdout_sink_mt::log(msg);
}
