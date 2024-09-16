#pragma once

#include <string_view>
#include <type_traits>

#include "ecs/ecs.h"
#include "protegon/vector2.h"

namespace ptgn {

/*
 * Hash a string into a number.
 * @param string The string to hash.
 * @return Unique positive integer corresponding to the string.
 */
[[nodiscard]] inline std::size_t Hash(std::string_view string) {
	return std::hash<std::string_view>()(string);
}

[[nodiscard]] inline std::size_t Hash(const ecs::Entity& e) {
	return std::hash<ecs::Entity>()(e);
}

template <typename T>
[[nodiscard]] inline std::size_t Hash(const Vector2<T>& vector) {
	return std::hash<Vector2<T>>()(vector);
}

} // namespace ptgn