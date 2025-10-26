#pragma once

#include <string_view>
#include <type_traits>

#include "core/util/concepts.h"

namespace ptgn {

class Entity;

template <Arithmetic T>
struct Vector2;

//[[nodiscard]] inline std::size_t Hash(std::string_view string) {
//	return std::hash<std::string_view>()(string);
//}

/*
 * Hash a string into a number.
 * @param string The string to hash.
 * @return Unique positive integer corresponding to the string.
 */
[[nodiscard]] constexpr std::size_t Hash(std::string_view string) {
	if (string.empty()) {
		return 0;
	}
	std::size_t hash{ 5381 };
	for (const auto& c : string) {
		hash = (33 * hash) ^ static_cast<std::size_t>(c);
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

template <Arithmetic T>
[[nodiscard]] std::size_t Hash(const Vector2<T>& vector);

[[nodiscard]] std::size_t Hash(const Entity& entity);

} // namespace ptgn