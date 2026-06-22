#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ptgn {

namespace fs = std::filesystem;
using path	 = fs::path;

void EnsureDirectory(const path& path);
[[nodiscard]] std::string FileToString(const path& file);
path GetWorkingDirectory();
std::string GetExtension(const path& file);
bool HasExtension(const path& file, std::string_view extension);
[[nodiscard]] path MergePaths(const path& path_A, const path& path_B);
[[nodiscard]] bool FileExists(const path& file_path);
[[nodiscard]] bool DirectoryExists(const path& directory_path);
bool IsFilePath(std::string_view potential_file_path);
bool IsDirectoryPath(std::string_view potential_directory_path);
path GetAbsolutePath(const path& relative_path);
path GetRelativePath(const path& absolute_path);
path GetAssetRoot();

enum class FileWriteError {
	OpenFailed,
	WriteFailed
};

[[nodiscard]] std::vector<std::uint8_t> ReadBinary(const path& file);

std::expected<void, FileWriteError> WriteBinary(
	const path& file_path, std::span<const std::uint8_t> bytes
);

} // namespace ptgn
