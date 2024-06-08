#pragma once

#include <cstdlib>
#include <functional>
#include <string>
#include <cstring>

namespace ptgn {

/*
* Hash a string into a number.
* @param c_string The string to hash.
* @return Unique positive integer corresponding to the string.
*/
[[nodiscard]] inline std::size_t Hash(const char* c_string) {
    return std::hash<std::string_view>()(std::string_view(c_string, std::strlen(c_string)));
}

} // namespace ptgn