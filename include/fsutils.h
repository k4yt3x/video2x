#ifndef FSUTILS_H
#define FSUTILS_H

#include <string>

#if _WIN32
#include <windows.h>
#include <cwchar>
#else
#include <linux/limits.h>
#include <unistd.h>
#include <cstring>
#endif

#if _WIN32
typedef std::wstring path_t;
#define PATHSTR(X) L##X
#else
typedef std::string path_t;
#define PATHSTR(X) X
#endif

#if _WIN32
static path_t get_executable_directory() {
    wchar_t filepath[256];
    GetModuleFileNameW(NULL, filepath, 256);

    wchar_t *backslash = wcsrchr(filepath, L'\\');
    backslash[1] = L'\0';

    return path_t(filepath);
}
#else   // _WIN32
static path_t get_executable_directory() {
    char filepath[PATH_MAX];
    if (readlink("/proc/self/exe", filepath, PATH_MAX) == -1) {
        return path_t("");
    }

    char *slash = strrchr(filepath, '/');
    slash[1] = '\0';

    return path_t(filepath);
}
#endif  // _WIN32

static bool filepath_is_readable(const path_t &path) {
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

static path_t get_full_path(const path_t &path) {
    if (filepath_is_readable(path)) {
        return path;
    }

    return get_executable_directory() + path;
}

#endif  // FSUTILS_H
