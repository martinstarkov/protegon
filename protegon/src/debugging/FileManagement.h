#pragma once

#include <fstream> // std::ifstream

namespace ptgn {

inline bool FileExists(const char* file) {
	std::ifstream test{ file };
	return static_cast<bool>(test);
}

} // namespace ptgn