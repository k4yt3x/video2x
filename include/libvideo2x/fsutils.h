#pragma once

#include <filesystem>
#include <string>

namespace video2x {
namespace fsutils {

#ifdef _WIN32
typedef wchar_t CharType;
#define STR(x) L##x
#else
typedef char CharType;
#define STR(x) x
#endif

#ifdef _WIN32
typedef std::wstring StringType;
#else
typedef std::string StringType;
#endif

bool filepath_is_readable(const std::filesystem::path &path);

std::filesystem::path find_resource_file(const std::filesystem::path &path);

std::string path_to_u8string(const std::filesystem::path &path);

std::string wstring_to_u8string(const fsutils::StringType &wstr);

fsutils::StringType path_to_string_type(const std::filesystem::path &path);

fsutils::StringType to_string_type(int value);

}  // namespace fsutils
}  // namespace video2x
