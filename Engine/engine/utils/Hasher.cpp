#include "Hasher.h"

#include <functional> // std::hash
#include <string> // std::string_view

namespace engine {

std::size_t Hasher::HashCString(const char* s) {
	return std::hash<std::string_view>()(std::string_view(s, std::strlen(s)));
}

} // namespace engine