#pragma once

#include <fstream>

namespace engine {

inline bool FileExists(const char* file) {
	std::ifstream test{ file };
	return test ? true : false;
}

} // namespace engine