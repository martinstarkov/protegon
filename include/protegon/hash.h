#pragma once

#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>

namespace ptgn {

/*
 * Hash a string into a number.
 * @param string The string to hash.
 * @return Unique positive integer corresponding to the string.
 */
[[nodiscard]] inline std::size_t Hash(const std::string_view& string) {
	return std::hash<std::string_view>()(string);
}

/*
 * Hash a C string into a number.
 * @param c_string The string to hash.
 * @return Unique positive integer corresponding to the string.
 */
[[nodiscard]] inline std::size_t Hash(const char* c_string) {
	return std::hash<std::string_view>()(std::string_view(c_string, std::strlen(c_string)));
}

[[nodiscard]] inline std::size_t Hash(const ecs::Entity& e) {
	return std::hash<ecs::Entity>()(e);
}

template <typename T>
[[nodiscard]] inline std::size_t Hash(const Vector2<T>& vector) {
	return std::hash<Vector2<T>>()(vector);
}

} // namespace ptgn