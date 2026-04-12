#include "core/util/file.h"

#include <filesystem>
#include <fstream>
#include <ios>
#include <ostream>
#include <sstream>
#include <string>

#include "core/assert.h"
#include "serialization/json/json.h"

namespace ptgn {

std::string FileToString(const path& file) {
	PTGN_ASSERT(FileExists(file), "Cannot convert non-existent file to string: ", file.string());
	// Source: https://stackoverflow.com/a/2602258
	std::ifstream f(GetAbsolutePath(file), std::ios::in | std::ios::binary);
	// TODO: Add further checks for file being opened correctly.
	PTGN_ASSERT(f, "Could not open file to convert it to string: ", file.string());
	std::stringstream buffer;
	buffer << f.rdbuf();
	return buffer.str();
}

path GetWorkingDirectory() {
	return fs::current_path();
}

path MergePaths(const path& pathA, const path& pathB) {
	return pathA / pathB;
}

bool IsFilePath(const std::string& s) {
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

bool IsDirectoryPath(const std::string& s) {
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

bool FileExists(const path& file_path) {
	return fs::exists(file_path) || fs::exists(GetAbsolutePath(file_path));
}

bool DirectoryExists(const path& directory_path) {
	return fs::is_directory(directory_path) || fs::is_directory(GetAbsolutePath(directory_path));
}

path GetAbsolutePath(const path& relative_file_path) {
	return (path{ PTGN_ROOT } / relative_file_path).lexically_normal();
}

path GetRelativePath(const path& absolute_file_path) {
	return absolute_file_path.relative_path();
}

void to_json(json& j, const path& p) {
	j = p.string();
}

void from_json(const json& j, path& p) {
	p = j.template get<std::string>();
}

} // namespace ptgn