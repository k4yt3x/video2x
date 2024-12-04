#include <iostream>

#ifdef _WIN32
#include <Windows.h>
#include <conio.h>
#else
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

#include <spdlog/spdlog.h>

#include "argparse.h"
#include "logging.h"
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
int wmain(int argc, wchar_t *argv[]) {
    // Set console output code page to UTF-8
    SetConsoleOutputCP(CP_UTF8);

    // Enable ANSI escape codes
    HANDLE console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD console_mode = 0;
    GetConsoleMode(console_handle, &console_mode);
    console_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(console_handle, console_mode);
#else
int main(int argc, char **argv) {
#endif
    // Initialize arguments structures
    Arguments arguments;
    ProcessorConfig proc_cfg;
    EncoderConfig enc_cfg;

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
    VideoProcessor video_processor = VideoProcessor(
        proc_cfg,
        enc_cfg,
        arguments.vk_device_index,
        arguments.hw_device_type,
        arguments.log_level,
        arguments.benchmark
    );

    // Register a newline-safe log callback for FFmpeg
    av_log_set_callback(newline_safe_ffmpeg_log_callback);

    // Create a thread for video processing
    int proc_ret = 0;
    std::atomic<bool> completed = false;  // Use atomic for thread-safe updates
    std::thread processing_thread([&]() {
        proc_ret = video_processor.process(arguments.in_fname, arguments.out_fname);
        completed.store(true, std::memory_order_relaxed);
    });
    spdlog::info("Press [space] to pause/resume, [q] to abort.");

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
            // Toggle pause state
            {
                if (video_processor.is_paused()) {
                    video_processor.resume();
                } else {
                    video_processor.pause();
                }
                if (video_processor.is_paused()) {
                    std::cout
                        << "\r\033[KProcessing paused; press [space] to resume, [q] to abort.";
                    std::cout.flush();
                    timer.pause();
                } else {
                    std::cout << "\r\033[KProcessing resumed.";
                    std::cout.flush();
                    timer.resume();
                }
                newline_required.store(true);
            }
        } else if (ch == 'q' || ch == 'Q') {
            // Abort processing
            if (newline_required.load()) {
                putchar('\n');
            }
            spdlog::warn("Aborting gracefully; press Ctrl+C to terminate forcefully.");
            {
                video_processor.abort();
                newline_required.store(false);
            }
            break;
        }

        // Display progress
        if (!arguments.no_progress) {
            int64_t processed_frames, total_frames;
            bool paused;
            {
                processed_frames = video_processor.get_processed_frames();
                total_frames = video_processor.get_total_frames();
                paused = video_processor.is_paused();
            }
            if (!paused && (total_frames > 0 || processed_frames > 0)) {
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
                newline_required.store(true);
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

    // Print a newline if progress bar was displayed
    if (newline_required.load()) {
        std::cout << '\n';
    }

    // Print final message based on processing result
    if (video_processor.is_aborted()) {
        spdlog::warn("Video processing aborted");
        return 2;
    } else if (proc_ret != 0) {
        spdlog::critical("Video processing failed with error code {}", proc_ret);
        return 1;
    } else {
        spdlog::info("Video processed successfully");
    }

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
    std::cout << "Total time taken: " << std::setw(2) << std::setfill('0') << hours_elapsed << ":"
              << std::setw(2) << std::setfill('0') << minutes_elapsed << ":" << std::setw(2)
              << std::setfill('0') << seconds_elapsed << std::endl;
    std::cout << "Average processing speed: " << std::fixed << std::setprecision(2)
              << average_speed_fps << " FPS" << std::endl;

    // Print additional information if not in benchmark mode
    if (!arguments.benchmark) {
        std::cout << "Output written to: " << arguments.out_fname.u8string() << std::endl;
    }

    return 0;
}
