#include "fsutils.h"

#if _WIN32
#include <windows.h>
#include <cwchar>
#else
#include <unistd.h>
#include <cstring>
#endif

#include <spdlog/spdlog.h>

#if _WIN32
static std::filesystem::path get_executable_directory() {
    std::vector<wchar_t> filepath(MAX_PATH);

    // Get the executable path, expanding the buffer if necessary
    DWORD size = GetModuleFileNameW(NULL, filepath.data(), static_cast<DWORD>(filepath.size()));
    if (size == 0) {
        spdlog::error("Error getting executable path: {}", GetLastError());
        return std::filesystem::path();
    }

    // Resize the buffer if necessary
    while (size >= filepath.size()) {
        filepath.resize(filepath.size() * 2);
        size = GetModuleFileNameW(NULL, filepath.data(), static_cast<DWORD>(filepath.size()));
        if (size == 0) {
            spdlog::error("Error getting executable path: {}", GetLastError());
            return std::filesystem::path();
        }
    }

    // Create a std::filesystem::path from the filepath and return its parent path
    std::filesystem::path execpath(filepath.data());
    return execpath.parent_path();
}
#else   // _WIN32
static std::filesystem::path get_executable_directory() {
    std::error_code ec;
    std::filesystem::path filepath = std::filesystem::read_symlink("/proc/self/exe", ec);

    if (ec) {
        spdlog::error("Error reading /proc/self/exe: {}", ec.message());
        return std::filesystem::path();
    }

    return filepath.parent_path();
}
#endif  // _WIN32

bool filepath_is_readable(const std::filesystem::path &path) {
#if _WIN32
    FILE *fp = _wfopen(path.c_str(), L"rb");
#else   // _WIN32
    FILE *fp = fopen(path.c_str(), "rb");
#endif  // _WIN32
    if (!fp) {
        return false;
    }

    fclose(fp);
    return true;
}

std::filesystem::path find_resource_file(const std::filesystem::path &path) {
    if (filepath_is_readable(path)) {
        return path;
    }

    if (filepath_is_readable(std::filesystem::path("/usr/share/video2x/") / path)) {
        return std::filesystem::path("/usr/share/video2x/") / path;
    }

    return get_executable_directory() / path;
}

std::string path_to_u8string(const std::filesystem::path &path) {
#if _WIN32
    std::wstring wide_path = path.wstring();
    int buffer_size =
        WideCharToMultiByte(CP_UTF8, 0, wide_path.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (buffer_size == 0) {
        return std::string();
    }
    std::vector<char> buffer(buffer_size);
    WideCharToMultiByte(
        CP_UTF8, 0, wide_path.c_str(), -1, buffer.data(), buffer_size, nullptr, nullptr
    );
    return std::string(buffer.data());
#else
    return path.string();
#endif
}

StringType path_to_string_type(const std::filesystem::path &path) {
#if _WIN32
    return path.wstring();
#else
    return path.string();
#endif
}

StringType to_string_type(int value) {
#if _WIN32
    return std::to_wstring(value);
#else
    return std::to_string(value);
#endif
}
