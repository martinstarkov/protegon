#pragma once

#include <filesystem>
#include <fstream>
#include <string>

#include "serialization/json/fwd.h"

namespace ptgn {

namespace fs = std::filesystem;
using path	 = std::filesystem::path;

[[nodiscard]] std::string FileToString(const path& file);
[[nodiscard]] path GetWorkingDirectory();
[[nodiscard]] path MergePaths(const path& path_A, const path& path_B);
[[nodiscard]] bool FileExists(const path& file_path);
[[nodiscard]] bool DirectoryExists(const path& directory_path);
[[nodiscard]] path GetAbsolutePath(const path& relative_file_path);
[[nodiscard]] path GetRelativePath(const path& absolute_file_path);

void to_json(json& j, const path& p);
void from_json(const json& j, path& p);

} // namespace ptgn
