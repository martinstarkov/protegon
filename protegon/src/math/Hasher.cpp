#include "Hasher.h"

#include <functional> // std::hash
#include <string> // std::string_view
#include <cstring> // std::strlen

namespace ptgn {

std::size_t Hasher::HashCString(const char* c_string) {
	return std::hash<std::string_view>()(std::string_view(c_string, std::strlen(c_string)));
}

} // namespace ptgn