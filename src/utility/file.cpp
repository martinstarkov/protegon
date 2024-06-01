#include "protegon/file.h"

#include <cassert>
#include <sstream>
#include <fstream>

namespace ptgn {

std::string FileToString(const fs::path& file) {
    // Source: https://stackoverflow.com/a/2602258
    std::ifstream f(file, std::ios::in | std::ios::binary);
	assert(f && "Could not open file to convert it to string");
    std::stringstream buffer;
    buffer << f.rdbuf();
    return buffer.str();
}

fs::path GetExecutablePath() {
   return fs::current_path();
}

fs::path GetExecutableDirectory() {
    return GetExecutablePath().parent_path();
}

fs::path MergePaths(const fs::path& pathA, const fs::path& pathB) {
  return pathA / pathB;
}

bool FileExists(const fs::path& file_path) {
    return fs::exists(file_path);
}

fs::path GetAbsolutePath(const fs::path& relative_file_path) {
    return fs::current_path() / relative_file_path;
}

fs::path GetRelativePath(const fs::path& absolute_file_path) {
    return absolute_file_path.relative_path();
}

} // namespace ptgn
