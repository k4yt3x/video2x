#include "fsutils.h"

#if _WIN32
#include <Windows.h>
#include <cwchar>
#else
#include <unistd.h>
#include <cstring>
#endif

#include <spdlog/spdlog.h>

#include "logger_manager.h"

namespace video2x {
namespace fsutils {

#if _WIN32
static std::filesystem::path get_executable_directory() {
    std::vector<wchar_t> filepath(MAX_PATH);

    // Get the executable path, expanding the buffer if necessary
    DWORD size = GetModuleFileNameW(nullptr, filepath.data(), static_cast<DWORD>(filepath.size()));
    if (size == 0) {
        logger()->error("Error getting executable path: {}", GetLastError());
        return std::filesystem::path();
    }

    // Resize the buffer if necessary
    while (size >= filepath.size()) {
        filepath.resize(filepath.size() * 2);
        size = GetModuleFileNameW(nullptr, filepath.data(), static_cast<DWORD>(filepath.size()));
        if (size == 0) {
            logger()->error("Error getting executable path: {}", GetLastError());
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
        logger()->error("Error reading /proc/self/exe: {}", ec.message());
        return std::filesystem::path();
    }

    return filepath.parent_path();
}
#endif  // _WIN32

bool file_is_readable(const std::filesystem::path& path) {
#if _WIN32
    FILE* fp = _wfopen(path.c_str(), L"rb");
#else
    FILE* fp = fopen(path.c_str(), "rb");
#endif
    if (fp == nullptr) {
        return false;
    }

    fclose(fp);
    return true;
}

std::optional<std::filesystem::path> find_resource(const std::filesystem::path& resource) {
    // Build a list of candidate directories
    std::vector<std::filesystem::path> candidates;

    // 1. The resource's path as provided
    candidates.push_back(resource);

#ifndef _WIN32
    // 2. AppImage's mounted directory
    if (const char* appdir = std::getenv("APPDIR")) {
        candidates.push_back(
            std::filesystem::path(appdir) / "usr" / "share" / "video2x" / resource
        );
    }

    // 3. The Linux standard local data directory
    candidates.push_back(std::filesystem::path("/usr/local/share/video2x") / resource);

    // 4. The Linux standard data directory
    candidates.push_back(std::filesystem::path("/usr/share/video2x") / resource);
#endif

    // 5. The executable's parent directory
    candidates.push_back(get_executable_directory() / resource);

    // Iterate over the candidate directories and return the first readable file
    for (const auto& candidate : candidates) {
        if (file_is_readable(candidate) || std::filesystem::is_directory(candidate)) {
            return candidate;
        }
    }

    // Return nullopt if the resource was not found
    return std::nullopt;
}

std::string path_to_u8string(const std::filesystem::path& path) {
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

#ifdef _WIN32
std::string wstring_to_u8string(const std::wstring& wstr) {
    if (wstr.empty()) {
        return std::string();
    }
    int size_needed = WideCharToMultiByte(
        CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr
    );
    std::string converted_str(size_needed, 0);
    WideCharToMultiByte(
        CP_UTF8,
        0,
        wstr.data(),
        static_cast<int>(wstr.size()),
        &converted_str[0],
        size_needed,
        nullptr,
        nullptr
    );
    return converted_str;
}
#else   // _WIN32
std::string wstring_to_u8string(const std::string& str) {
    return str;
}
#endif  // _WIN32

fsutils::StringType path_to_string_type(const std::filesystem::path& path) {
#if _WIN32
    return path.wstring();
#else
    return path.string();
#endif
}

fsutils::StringType to_string_type(int value) {
#if _WIN32
    return std::to_wstring(value);
#else
    return std::to_string(value);
#endif
}

}  // namespace fsutils
}  // namespace video2x
