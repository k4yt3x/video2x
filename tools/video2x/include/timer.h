#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

class Timer {
   public:
    Timer();
    ~Timer();

    void start();
    void pause();
    void resume();
    void stop();

    bool is_running() const;
    bool is_paused() const;
    int64_t get_elapsed_time() const;

   private:
    std::atomic<bool> running;
    std::atomic<bool> paused;
    std::thread timer_thread;
    int64_t elapsed_time;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point pause_start_time;

    void update_elapsed_time();
};
