#pragma once

#include <filesystem> // std::filesystem
#include <fstream>    // std::ifstream
#include <string>

namespace ptgn {

std::string GetExecutablePath();
std::string GetExecutableDirectory();
std::string MergePaths(std::string path_A, std::string path_B);
bool FileExists (const std::string& file_path);
std::string GetAbsolutePath(const std::string& relative_file_path);

} // namespace ptgn
