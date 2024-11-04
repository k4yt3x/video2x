#ifndef FSUTILS_H
#define FSUTILS_H

#include <filesystem>
#include <string>

#include "char_defs.h"

bool filepath_is_readable(const std::filesystem::path &path);

std::filesystem::path find_resource_file(const std::filesystem::path &path);

std::string path_to_u8string(const std::filesystem::path &path);

StringType path_to_string_type(const std::filesystem::path &path);

StringType to_string_type(int value);

#endif  // FSUTILS_H
