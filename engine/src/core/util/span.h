#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <functional>
#include <span>
#include <vector>

namespace ptgn {

/// @return How many bits the contents of the vector take up.
template <typename T>
std::size_t Sizeof(const std::vector<T>& vector) {
	return sizeof(T) * vector.size();
}

/// @return How many bits the contents of the array take up.
template <typename T, std::size_t I>
constexpr std::size_t Sizeof(const std::array<T, I>& array) {
	return sizeof(T) * array.size();
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
	result.insert(result.end(), v1.begin(), v1.end());
	result.insert(result.end(), v2.begin(), v2.end());
	(result.insert(result.end(), vectors.begin(), vectors.end()), ...);
	return result;
}

/// @brief Combine two vectors into one.
/// @return A new vector containing the elements of both vectors.
template <typename T>
[[nodiscard]] auto VectorConcat(const std::vector<T>& v1, const std::vector<T>& v2) {
	std::vector<T> result;
	result.reserve(v1.size() + v2.size());
	result.insert(result.end(), v1.begin(), v1.end());
	result.insert(result.end(), v2.begin(), v2.end());
	return result;
}

template <typename T>
void VectorRemoveDuplicates(std::vector<T>& v) {
	std::sort(v.begin(), v.end()); // NOSONAR
	auto last{ std::ranges::unique(v) };
	v.erase(last.begin(), last.end());
}

template <typename T, typename Pred>
	requires std::predicate<Pred&, const T&, const T&>
bool ContainsDuplicates(std::span<const T> values, Pred pred) {
	auto count{ values.size() };

	for (auto i{ 0uz }; i < count; ++i) {
		for (auto j{ i + 1 }; j < count; ++j) {
			if (std::invoke(pred, values[i], values[j])) {
				return true;
			}
		}
	}

	return false;
}

/// @brief Swaps vector elements if they both exist in the vector.
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
