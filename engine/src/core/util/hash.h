#pragma once

#include <algorithm>
#include <concepts>
#include <functional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>

#include "core/util/type_info.h"

namespace ptgn {

template <typename T>
concept StringLikeHashInput =
	requires(const std::remove_cvref_t<T>& value) { std::string_view{ value }; };

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

[[nodiscard]] constexpr std::size_t Hash(const char* string) {
	return string ? Hash(std::string_view{ string }) : 0;
}

template <std::size_t N>
[[nodiscard]] constexpr std::size_t Hash(const char (&string)[N]) {
	return Hash(std::string_view{ string, N - 1 });
}

[[nodiscard]] inline std::size_t Hash(const std::string& string) {
	return Hash(std::string_view{ string });
}

/// @brief Hash a type into a number.
template <typename T>
[[nodiscard]] constexpr std::size_t Hash() {
	return Hash(type_name<T>());
}

template <typename T>
concept Hashable = requires(const std::remove_cvref_t<T>& value) {
	{ std::hash<std::remove_cvref_t<T>>{}(value) } -> std::convertible_to<std::size_t>;
};

template <Hashable T>
	requires(!StringLikeHashInput<T>)
std::size_t Hash(const T& value) {
	using U = std::remove_cvref_t<T>;
	return std::hash<U>{}(value);
}

namespace impl {

inline void HashCombine(std::size_t& hash, std::size_t other_hash) {
	hash ^= other_hash + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
}

template <Hashable T>
void HashValue(std::size_t& hash, const T& value) {
	return impl::HashCombine(hash, Hash(value));
}

} // namespace impl

template <std::ranges::input_range R>
	requires Hashable<std::ranges::range_value_t<R>>
std::size_t Hash(const R& value) {
	std::size_t hash{ 0 };

	std::ranges::for_each(value, [&](const auto& element) {
		impl::HashCombine(hash, Hash(element));
	});

	return hash;
}

template <Hashable... Ts>
	requires(sizeof...(Ts) > 1)
std::size_t Hash(const Ts&... values) {
	std::size_t hash{ 0 };
	(impl::HashValue(hash, values), ...);
	return hash;
}

} // namespace ptgn