#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace ptgn {

namespace fs = std::filesystem;
using path	 = fs::path;

void EnsureDirectory(const path& path);
[[nodiscard]] std::string FileToString(const path& file);
path GetWorkingDirectory();
[[nodiscard]] path MergePaths(const path& path_A, const path& path_B);
[[nodiscard]] bool FileExists(const path& file_path);
[[nodiscard]] bool DirectoryExists(const path& directory_path);
[[nodiscard]] bool IsFilePath(std::string_view potential_file_path);
[[nodiscard]] bool IsDirectoryPath(std::string_view potential_directory_path);
path GetAbsolutePath(const path& relative_path);
path GetRelativePath(const path& absolute_path);
path GetAssetRoot();

} // namespace ptgn
