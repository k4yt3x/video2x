#include <iostream>

#ifdef _WIN32
#include <Windows.h>
#include <conio.h>
#else
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

#include <libvideo2x/logger_manager.h>

#include "argparse.h"
#include "newline_safe_sink.h"
#include "timer.h"

// Set UNIX terminal input to non-blocking mode
#ifndef _WIN32
void set_nonblocking_input(bool enable) {
    static termios oldt, newt;
    if (enable) {
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= static_cast<unsigned int>(~(ICANON | ECHO));
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    } else {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        fcntl(STDIN_FILENO, F_SETFL, 0);
    }
}
#endif

std::tuple<int, int, int> calculate_time_components(int time_elapsed) {
    int hours_elapsed = time_elapsed / 3600;
    int minutes_elapsed = (time_elapsed % 3600) / 60;
    int seconds_elapsed = time_elapsed % 60;
    return {hours_elapsed, minutes_elapsed, seconds_elapsed};
}

#ifdef _WIN32
int wmain(int argc, wchar_t* argv[]) {
    // Set console output code page to UTF-8
    SetConsoleOutputCP(CP_UTF8);

    // Enable ANSI escape codes
    HANDLE console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD console_mode = 0;
    GetConsoleMode(console_handle, &console_mode);
    console_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(console_handle, console_mode);
#else
int main(int argc, char** argv) {
#endif
    // Initialize newline-safe logger with custom formatting pattern
    std::shared_ptr<newline_safe_sink> logger_sink = std::make_shared<newline_safe_sink>();
    std::vector<spdlog::sink_ptr> sinks = {logger_sink};
    if (!video2x::logger_manager::LoggerManager::instance().reconfigure_logger(
            "video2x", sinks, "[%Y-%m-%d %H:%M:%S] [%^%l%$] %v"
        )) {
        std::cerr << "Error: Failed to configure logger." << std::endl;
        return 1;
    }

    // Initialize argument and configuration structs
    Arguments arguments;
    video2x::processors::ProcessorConfig proc_cfg;
    video2x::encoder::EncoderConfig enc_cfg;

    // Parse command line arguments
    int parse_ret = parse_args(argc, argv, arguments, proc_cfg, enc_cfg);

    // Return if parsing failed
    if (parse_ret < 0) {
        return parse_ret;
    }

    // Return if help message or version info was displayed
    if (parse_ret > 0) {
        return 0;
    }

    // Create video processor object
    video2x::VideoProcessor video_processor = video2x::VideoProcessor(
        proc_cfg, enc_cfg, arguments.vk_device_index, arguments.hw_device_type, arguments.benchmark
    );

    // Create a thread for video processing
    int proc_ret = 0;
    std::atomic<bool> completed = false;  // Use atomic for thread-safe updates
    std::thread processing_thread([&]() {
        proc_ret = video_processor.process(arguments.in_fname, arguments.out_fname);
        completed.store(true, std::memory_order_relaxed);
    });
    video2x::logger()->info("Press [space] to pause/resume, [q] to abort.");

    // Setup timer
    Timer timer;
    timer.start();

    // Enable non-blocking input
#ifndef _WIN32
    set_nonblocking_input(true);
#endif

    // Main thread loop to display progress and handle input
    while (true) {
        if (completed.load()) {
            break;
        }

        // Check for key presses
        int ch = -1;

        // Check for key press
#ifdef _WIN32
        if (_kbhit()) {
            ch = _getch();
        }
#else
        ch = getchar();
#endif

        if (ch == ' ' || ch == '\n') {
            {
                // Toggle pause state
                if (video_processor.get_state() == video2x::VideoProcessorState::Paused) {
                    video_processor.resume();
                } else {
                    video_processor.pause();
                }

                // Print message based on current state and pause/resume the timer
                if (video_processor.get_state() == video2x::VideoProcessorState::Paused) {
                    std::cout
                        << "\r\033[KProcessing paused; press [space] to resume, [q] to abort.";
                    std::cout.flush();
                    timer.pause();
                } else {
                    std::cout << "\r\033[KProcessing resumed.";
                    std::cout.flush();
                    timer.resume();
                }
                logger_sink->set_needs_newline(true);
            }
        } else if (ch == 'q' || ch == 'Q') {
            // Abort processing
            video2x::logger()->warn("Aborting gracefully; press Ctrl+C to terminate forcefully.");
            video_processor.abort();
            break;
        }

        // Display progress
        if (!arguments.no_progress) {
            int64_t processed_frames = video_processor.get_processed_frames();
            int64_t total_frames = video_processor.get_total_frames();

            // Print the progress bar if processing is not paused
            if (video_processor.get_state() != video2x::VideoProcessorState::Paused &&
                (total_frames > 0 || processed_frames > 0)) {
                double percentage = total_frames > 0 ? static_cast<double>(processed_frames) *
                                                           100.0 / static_cast<double>(total_frames)
                                                     : 0.0;
                int time_elapsed = static_cast<int>(timer.get_elapsed_time() / 1000);

                // Calculate hours, minutes, and seconds elapsed
                auto [hours_elapsed, minutes_elapsed, seconds_elapsed] =
                    calculate_time_components(time_elapsed);

                // Calculate estimated time remaining
                int64_t frames_remaining = total_frames - processed_frames;
                double processing_rate = static_cast<double>(processed_frames) / time_elapsed;
                int time_remaining =
                    static_cast<int>(static_cast<double>(frames_remaining) / processing_rate);
                time_remaining = std::max<int>(time_remaining, 0);

                // Calculate hours, minutes, and seconds remaining
                auto [hours_remaining, minutes_remaining, seconds_remaining] =
                    calculate_time_components(time_remaining);

                // Print the progress bar
                std::cout << "\r\033[Kframe=" << processed_frames << "/" << total_frames << " ("
                          << std::fixed << std::setprecision(2) << percentage
                          << "%); fps=" << std::fixed << std::setprecision(2) << processing_rate
                          << "; elapsed=" << std::setw(2) << std::setfill('0') << hours_elapsed
                          << ":" << std::setw(2) << std::setfill('0') << minutes_elapsed << ":"
                          << std::setw(2) << std::setfill('0') << seconds_elapsed
                          << "; remaining=" << std::setw(2) << std::setfill('0') << hours_remaining
                          << ":" << std::setw(2) << std::setfill('0') << minutes_remaining << ":"
                          << std::setw(2) << std::setfill('0') << seconds_remaining;
                std::cout.flush();
                logger_sink->set_needs_newline(true);
            }
        }

        // Sleep for 100ms
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Restore terminal to blocking mode
#ifndef _WIN32
    set_nonblocking_input(false);
#endif

    // Join the processing thread to ensure it completes before exiting
    processing_thread.join();

    // Print final message based on processing result
    if (video_processor.get_state() == video2x::VideoProcessorState::Aborted) {
        video2x::logger()->warn("Video processing aborted");
        return 2;
    } else if (proc_ret != 0 ||
               video_processor.get_state() == video2x::VideoProcessorState::Failed) {
        video2x::logger()->critical("Video processing failed with error code {}", proc_ret);
        return 1;
    } else {
        video2x::logger()->info("Video processed successfully");
    }

    // Print the processing summary if the log level is info or lower
    if (video2x::logger()->level() <= spdlog::level::info) {
        // Calculate statistics
        int64_t processed_frames = video_processor.get_processed_frames();
        int time_elapsed = static_cast<int>(timer.get_elapsed_time() / 1000);
        auto [hours_elapsed, minutes_elapsed, seconds_elapsed] =
            calculate_time_components(time_elapsed);
        float average_speed_fps = static_cast<float>(processed_frames) /
                                  (time_elapsed > 0 ? static_cast<float>(time_elapsed) : 1);

        // Print processing summary
        std::cout << "====== Video2X " << (arguments.benchmark ? "Benchmark" : "Processing")
                  << " summary ======" << std::endl;
        std::cout << "Video file processed: " << arguments.in_fname.u8string() << std::endl;
        std::cout << "Total frames processed: " << processed_frames << std::endl;
        std::cout << "Total time taken: " << std::setw(2) << std::setfill('0') << hours_elapsed
                  << ":" << std::setw(2) << std::setfill('0') << minutes_elapsed << ":"
                  << std::setw(2) << std::setfill('0') << seconds_elapsed << std::endl;
        std::cout << "Average processing speed: " << std::fixed << std::setprecision(2)
                  << average_speed_fps << " FPS" << std::endl;

        // Print additional information if not in benchmark mode
        if (!arguments.benchmark) {
            std::cout << "Output written to: " << arguments.out_fname.u8string() << std::endl;
        }
    }

    return 0;
}
