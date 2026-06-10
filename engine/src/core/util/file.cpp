#include "core/util/file.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <ostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "core/assert.h"
#include "core/util/string.h"

namespace ptgn {

void EnsureDirectory(const path& path) {
	std::error_code ec;

	fs::create_directories(path, ec);

	PTGN_ASSERT(!ec, "Could not create directory: ", path.string(), ": ", ec.message());
}

std::string FileToString(const path& file) {
	PTGN_ASSERT(FileExists(file), "File does not exist: ", file.string());

	std::ifstream in{ GetAbsolutePath(file), std::ios::binary };
	PTGN_ASSERT(in, "Failed to open file: ", file.string());

	std::stringstream buffer;
	buffer << in.rdbuf();

	PTGN_ASSERT(in, "Failed to read file: ", file.string());

	return buffer.str();
}

std::vector<std::uint8_t> ReadBinary(const path& file) {
	PTGN_ASSERT(FileExists(file), "Binary file does not exist: ", file.string());

	std::vector<std::uint8_t> bytes(fs::file_size(file));

	std::ifstream in{ file, std::ios::binary };
	PTGN_ASSERT(in, "Failed to open binary file: ", file.string());

	in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));

	PTGN_ASSERT(in, "Failed to read binary file: ", file.string());

	return bytes;
}

std::expected<void, FileWriteError> WriteBinary(
	const path& file_path, std::span<const std::uint8_t> bytes
) {
	EnsureDirectory(file_path.parent_path());

	std::ofstream out{ file_path, std::ios::binary | std::ios::trunc };

	if (!out) {
		return std::unexpected(FileWriteError::OpenFailed);
	}

	out.write(
		reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())
	);

	if (!out) {
		return std::unexpected(FileWriteError::WriteFailed);
	}

	return {};
}

path GetWorkingDirectory() {
	return fs::current_path();
}

path MergePaths(const path& pathA, const path& pathB) {
	return pathA / pathB;
}

bool IsFilePath(std::string_view s) {
	if (s.empty()) {
		return false;
	}

	// directories must not count as files
	if (IsDirectoryPath(s)) {
		return false;
	}

	path p{ s };

	// extension strongly indicates a file
	if (p.has_extension()) {
		return true;
	}

	// path like "dir/file" (no extension but looks like a file)
	if (p.has_parent_path()) {
		return true;
	}

	return false;
}

bool IsDirectoryPath(std::string_view s) {
	if (s.empty()) {
		return false;
	}

	path p{ s };

	// explicit directory style
	if (s.ends_with('/') || s.ends_with('\\')) {
		return true;
	}

	// "." or ".."
	if (s == "." || s == "..") {
		return true;
	}

	// has separators but no extension
	if (p.has_parent_path() && !p.has_extension()) {
		return true;
	}

	return false;
}

std::string GetExtension(const path& file) {
	return ToLower(file.extension().string());
}

bool FileExists(const path& file) {
	return fs::exists(file) || fs::exists(GetAbsolutePath(file));
}

bool DirectoryExists(const path& directory_path) {
	return fs::is_directory(directory_path) || fs::is_directory(GetAbsolutePath(directory_path));
}

path GetAbsolutePath(const path& relative_path) {
	auto combined{ GetAssetRoot() / relative_path };

	if (fs::exists(combined)) {
		return combined.lexically_normal();
	}

	auto absolute_path{ GetWorkingDirectory() / relative_path };

	return absolute_path.lexically_normal();
}

path GetRelativePath(const path& absolute_path) {
	return absolute_path.relative_path();
}

path GetAssetRoot() {
	path root{ PTGN_ASSET_ROOT };
	return root.lexically_normal();
}

} // namespace ptgn