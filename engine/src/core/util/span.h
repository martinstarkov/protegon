#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <functional>
#include <ranges>
#include <span>
#include <vector>

namespace ptgn {

/// @return How many bits the contents of the span take up.
template <typename T>
std::size_t Sizeof(std::span<const T> container) {
	return sizeof(T) * container.size();
}

/// @brief Combine any number of arrays into one.
/// @return A new array containing the elements of all array.
template <typename Type, std::size_t... sizes>
[[nodiscard]] auto ArrayConcat(const std::array<Type, sizes>&... arrays) {
	std::array<Type, (sizes + ...)> result;
	std::size_t index{ 0 };

	((std::copy_n(arrays.begin(), sizes, result.begin() + index), index += sizes), ...);

	return result;
}

/// @brief Combine more than two vectors into one.
/// @return A new vector containing the elements of all vectors.
template <typename T, typename... TArgs> // NOSONAR
[[nodiscard]] auto VectorConcat(
	const std::vector<T>& v1, const std::vector<T>& v2, const TArgs&... vectors
) {
	std::vector<T> result;
	result.reserve(v1.size() + v2.size() + (vectors.size() + ...));
	result.append_range(v1);
	result.append_range(v2);
	(result.append_range(vectors), ...);
	return result;
}

/// @brief Combine two vectors into one.
/// @return A new vector containing the elements of both vectors.
template <typename T>
[[nodiscard]] auto VectorConcat(const std::vector<T>& v1, const std::vector<T>& v2) {
	std::vector<T> result;
	result.reserve(v1.size() + v2.size());
	result.append_range(v1);
	result.append_range(v2);
	return result;
}

template <typename T>
void VectorRemoveDuplicates(std::vector<T>& v) {
	std::sort(v.begin(), v.end()); // NOSONAR
	auto last{ std::ranges::unique(v) };
	v.erase(last.begin(), last.end());
}

template <std::ranges::forward_range TRange, typename TProj = std::identity>
constexpr bool ContainsDuplicates(TRange&& range, TProj proj = {}) {
	auto first{ std::ranges::begin(range) };
	auto last{ std::ranges::end(range) };

	for (auto it{ first }; it != last; ++it) {
		auto other{ it };
		++other;

		for (; other != last; ++other) {
			if (std::invoke(proj, *it) == std::invoke(proj, *other)) {
				return true;
			}
		}
	}

	return false;
}

/// @brief Swaps vector elements if both exist in the vector.
template <typename T>
void VectorSwapElements(std::vector<T>& v, const T& e1, const T& e2) {
	auto it1{ std::ranges::find(v, e1) };
	auto it2{ std::ranges::find(v, e2) };
	if (it1 == v.end() || it2 == v.end()) {
		return;
	}
	std::swap(*it1, *it2);
}

} // namespace ptgn
