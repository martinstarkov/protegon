#pragma once

#include <fstream> // std::ifstream

namespace engine {

inline bool FileExists(const char* file) {
	std::ifstream test{ file };
	return static_cast<bool>(test);
}

} // namespace engine