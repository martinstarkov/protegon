#pragma once

#include <string_view>
#include <type_traits>

#include "core/entity.h"
#include "math/vector2.h"

namespace ptgn {

//[[nodiscard]] inline std::size_t Hash(std::string_view string) {
//	return std::hash<std::string_view>()(string);
//}

/*
 * Hash a string into a number.
 * @param string The string to hash.
 * @return Unique positive integer corresponding to the string.
 */
[[nodiscard]] constexpr std::size_t Hash(std::string_view string) {
	std::size_t hash{ 5381 };
	for (const auto& c : string) {
		hash = (33 * hash) ^ c;
	}
	return hash;

	// Or alternatively:

	/*
	// FNV-1a hash algorithm (cross-compiler consistent)
	std::size_t hash				= 14695981039346656037ULL; // FNV_offset_basis
	constexpr std::size_t FNV_prime = 1099511628211ULL;

	for (char c : str) {
		hash ^= static_cast<std::uint8_t>(c); // XOR with byte
		hash *= FNV_prime;					  // Multiply by prime
	}

	return hash;
	*/
}

template <typename T>
[[nodiscard]] inline std::size_t Hash(const Vector2<T>& vector) {
	return std::hash<Vector2<T>>()(vector);
}

[[nodiscard]] inline std::size_t Hash(const Entity& entity) {
	return std::hash<Entity>()(entity);
}

} // namespace ptgn