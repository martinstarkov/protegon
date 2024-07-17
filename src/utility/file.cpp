#include "protegon/file.h"

#include <fstream>
#include <sstream>

#include "utility/debug.h"

namespace ptgn {

std::string FileToString(const path& file) {
	// Source: https://stackoverflow.com/a/2602258
	std::ifstream f(file, std::ios::in | std::ios::binary);
	// TODO: Add further checks for file being opened correctly.
	PTGN_CHECK(f, "Could not open file to convert it to string");
	std::stringstream buffer;
	buffer << f.rdbuf();
	return buffer.str();
}

path GetExecutablePath() {
	return fs::current_path();
}

path GetExecutableDirectory() {
	return GetExecutablePath().parent_path();
}

path MergePaths(const path& pathA, const path& pathB) {
	return pathA / pathB;
}

bool FileExists(const path& file_path) {
	return fs::exists(file_path);
}

path GetAbsolutePath(const path& relative_file_path) {
	return fs::current_path() / relative_file_path;
}

path GetRelativePath(const path& absolute_file_path) {
	return absolute_file_path.relative_path();
}

} // namespace ptgn
