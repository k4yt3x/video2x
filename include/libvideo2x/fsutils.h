#pragma once

#include <filesystem>
#include <string>

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

std::string wstring_to_u8string(const StringType &wstr);

StringType path_to_string_type(const std::filesystem::path &path);

StringType to_string_type(int value);
