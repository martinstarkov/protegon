#pragma once

#include <cstdlib> // std::size_t

namespace ptgn {

class Hasher {
public:
	/*
	* Hash a string into a number.
	* @param c_string The string to hash.
	* @return Unique positive integer corresponding to the string.
	*/
	static std::size_t HashCString(const char* c_string);
};

} // namespace ptgn