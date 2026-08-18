#include "core/util/file.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <istream>
#include <ostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <limits>

#include "core/assert.h"
#include "core/build_info.h"
#include "core/util/string.h"

namespace ptgn {

void EnsureDirectory(const path& path) {
	if (path.empty()) {
		PTGN_WARN("Cannot create directory for empty path");
		return;
	}

	std::error_code ec;
	fs::create_directories(path, ec);

	PTGN_ASSERT(
		!ec,
		"Could not create directory: ",
		path.string(),
		": ",
		ec.message()
	);
}

std::string FileToString(const path& file) {
	const path absolute_path{ GetAbsolutePath(file) };

	PTGN_ASSERT(
		FileExists(absolute_path),
		"File does not exist: ",
		absolute_path.string()
	);

	std::ifstream in{ absolute_path, std::ios::binary };

	PTGN_ASSERT(
		in,
		"Failed to open file: ",
		absolute_path.string()
	);

	std::stringstream buffer;
	buffer << in.rdbuf();

	PTGN_ASSERT(
		in,
		"Failed to read file: ",
		absolute_path.string()
	);

	return buffer.str();
}

std::vector<std::byte> ReadBinary(const path& file) {
	const path absolute_path{ GetAbsolutePath(file) };

	PTGN_ASSERT(
		FileExists(absolute_path),
		"Binary file does not exist: ",
		absolute_path.string()
	);

	std::uintmax_t file_size{
		fs::file_size(absolute_path)
	};

	PTGN_ASSERT(
		file_size <= std::numeric_limits<std::size_t>::max(),
		"Binary file is too large to load into memory: ",
		absolute_path.string()
	);

	std::vector<std::byte> bytes{
		static_cast<std::size_t>(file_size)
	};

	std::ifstream in{
		absolute_path,
		std::ios::binary
	};

	PTGN_ASSERT(
		in,
		"Failed to open binary file: ",
		absolute_path.string()
	);

	in.read(
		reinterpret_cast<char*>(bytes.data()), // NOSONAR
		static_cast<std::streamsize>(bytes.size())
	);

	PTGN_ASSERT(
		in,
		"Failed to read binary file: ",
		absolute_path.string()
	);

	return bytes;
}

std::expected<void, FileWriteError> WriteBinary(
	const path& file_path,
	std::span<const std::byte> bytes
) {
	EnsureDirectory(file_path.parent_path());

	std::ofstream out{
		file_path,
		std::ios::binary | std::ios::trunc
	};

	if (!out) {
		return std::unexpected(
			FileWriteError::OpenFailed
		);
	}

	out.write(
		reinterpret_cast<const char*>(bytes.data()), // NOSONAR
		static_cast<std::streamsize>(bytes.size())
	);

	if (!out) {
		return std::unexpected(
			FileWriteError::WriteFailed
		);
	}

	return {};
}

path GetWorkingDirectory() {
	std::error_code ec;
	path working_directory{ fs::current_path(ec) };

	if (!ec) {
		return working_directory.lexically_normal();
	}

	PTGN_WARN(
		"Failed to get working directory: ",
		ec.message(),
		". Falling back to runtime root."
	);

	return GetAssetRoot();
}

path MergePaths(
	const path& path_a,
	const path& path_b
) {
	return path_a / path_b;
}

bool IsFilePath(std::string_view value) {
	if (value.empty()) {
		return false;
	}

	if (IsDirectoryPath(value)) {
		return false;
	}

	path p{ value };

	if (p.has_extension()) {
		return true;
	}

	return p.has_parent_path();
}

bool IsDirectoryPath(std::string_view value) {
	if (value.empty()) {
		return false;
	}

	path p{ value };

	if (
		value.ends_with('/') ||
		value.ends_with('\\')
	) {
		return true;
	}

	if (
		value == "." ||
		value == ".."
	) {
		return true;
	}

	return
		p.has_parent_path() &&
		!p.has_extension();
}

std::string GetExtension(const path& file) {
	if (!file.has_extension()) {
		return "";
	}

	return ToLower(
		file.extension().string()
	);
}

bool HasExtension(
	const path& file,
	std::string_view extension
) {
	PTGN_ASSERT(
		extension.starts_with('.'),
		"Extension must start with a dot: ",
		extension
	);

	return
		file.has_extension() &&
		GetExtension(file) == extension;
}

bool FileExists(const path& file) {
	return fs::is_regular_file(file);
}

bool DirectoryExists(const path& directory) {
	return fs::is_directory(directory);
}

path GetAbsolutePath(const path& file) {
	if (file.is_absolute()) {
		return file.lexically_normal();
	}

	return (
		GetAssetRoot() /
		file
	).lexically_normal();
}

path GetRelativePath(const path& absolute_path) {
	return absolute_path.relative_path();
}

path GetAssetRoot() {
	return impl::GetBuildInfo()
		.runtime_root
		.lexically_normal();
}

} // namespace ptgn