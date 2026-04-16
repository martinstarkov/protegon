#pragma once

#include <string_view>

#include "core/util/type_info.h"

namespace ptgn {

/// @brief Hash a string into a number.
/// @param string The string to hash.
/// @return Unique positive integer corresponding to the string.
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

	// NOSONAR
	// FNV-1a hash algorithm (cross-compiler consistent)
	// std::size_t hash				= 14695981039346656037ULL; // FNV_offset_basis
	// constexpr std::size_t FNV_prime = 1099511628211ULL;
	// for (char c : str) {
	//	hash ^= static_cast<std::uint8_t>(c); // XOR with byte
	//	hash *= FNV_prime;					  // Multiply by prime
	//}
	// return hash;
}

/// @brief Hash a type into a number.
template <typename T>
[[nodiscard]] constexpr std::size_t Hash() {
	return Hash(type_name<T>());
}

} // namespace ptgn