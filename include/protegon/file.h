#pragma once

#include <filesystem>
#include <fstream>
#include <string>

namespace ptgn {

namespace fs = std::filesystem;
using path = std::filesystem::path;

std::string FileToString(const path& file);
path GetExecutablePath();
path GetExecutableDirectory();
path MergePaths(const path& path_A, const path& path_B);
bool FileExists (const path& file_path);
path GetAbsolutePath(const path& relative_file_path);
path GetRelativePath(const path& absolute_file_path);

} // namespace ptgn
