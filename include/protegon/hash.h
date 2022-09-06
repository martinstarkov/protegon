#pragma once

#include <cstdlib> // std::size_t
#include <functional> // std::hash
#include <string> // std::string_view
#include <cstring> // std::strlen

namespace ptgn {

/*
* Hash a string into a number.
* @param c_string The string to hash.
* @return Unique positive integer corresponding to the string.
*/
inline std::size_t Hash(const char* c_string) {
    return std::hash<std::string_view>()(std::string_view(c_string, std::strlen(c_string)));
}

} // namespace ptgn