#pragma once

#include <filesystem> // std::filesystem
#include <string>

namespace ptgn {

namespace fs = std::filesystem;

std::string FileToString(const fs::path& file);
fs::path GetExecutablePath();
fs::path GetExecutableDirectory();
fs::path MergePaths(const fs::path& path_A, const fs::path& path_B);
bool FileExists (const fs::path& file_path);
fs::path GetAbsolutePath(const fs::path& relative_file_path);
fs::path GetRelativePath(const fs::path& absolute_file_path);

} // namespace ptgn
