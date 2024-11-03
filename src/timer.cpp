#include "timer.h"

#include <sys/types.h>

Timer::Timer() : running(false), paused(false), elapsed_time(0) {}

Timer::~Timer() {
    stop();
}

void Timer::start() {
    if (running) {
        return;
    }

    running = true;
    paused = false;
    elapsed_time = 0;
    start_time = std::chrono::steady_clock::now();

    timer_thread = std::thread([this]() {
        while (running) {
            if (!paused) {
                update_elapsed_time();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
}

void Timer::pause() {
    if (running && !paused) {
        paused = true;
        pause_start_time = std::chrono::steady_clock::now();
    }
}

void Timer::resume() {
    if (running && paused) {
        paused = false;
        auto pause_end_time = std::chrono::steady_clock::now();
        auto pause_duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(pause_end_time - pause_start_time)
                .count();
        start_time += std::chrono::milliseconds(pause_duration);
    }
}

void Timer::stop() {
    running = false;
    if (timer_thread.joinable()) {
        timer_thread.join();
    }
    update_elapsed_time();
}

bool Timer::is_running() const {
    return running;
}

bool Timer::is_paused() const {
    return paused;
}

int64_t Timer::get_elapsed_time() const {
    return elapsed_time;
}

void Timer::update_elapsed_time() {
    if (running && !paused) {
        auto current_time = std::chrono::steady_clock::now();
        elapsed_time =
            std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time)
                .count();
    }
}
