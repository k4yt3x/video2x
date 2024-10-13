#ifndef FSUTILS_H
#define FSUTILS_H

#include <filesystem>

bool filepath_is_readable(const std::filesystem::path &path);

std::filesystem::path find_resource_file(const std::filesystem::path &path);

std::string path_to_string(const std::filesystem::path& path);

#endif  // FSUTILS_H
