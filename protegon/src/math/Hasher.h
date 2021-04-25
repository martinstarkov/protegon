#pragma once

#include <cstdlib> // std::size_t

namespace engine {

class Hasher {
public:
	// Turn a const char* into an integer.
	static std::size_t HashCString(const char* s);
};

} // namespace engine