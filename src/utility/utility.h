#pragma once

#include <array>
#include <unordered_set>
#include <vector>

namespace ptgn {

template <typename T>
static bool VectorContains(const std::vector<T>& container, const T& value) {
	return std::find(container.begin(), container.end(), value) != container.end();
}

template <typename Type, std::size_t... sizes>
static auto ConcatenateArrays(const std::array<Type, sizes>&... arrays) {
	std::array<Type, (sizes + ...)> result;
	std::size_t index{ 0 };

	((std::copy_n(arrays.begin(), sizes, result.begin() + index), index += sizes), ...);

	return result;
}

template <typename T, typename... TArgs>
static auto ConcatenateVectors(
	const std::vector<T>& v1, const std::vector<T>& v2, const TArgs&... vectors
) {
	std::vector<T> result;
	result.reserve(v1.size() + v2.size() + (vectors.size() + ...));
	result.insert(result.end(), v1.begin(), v1.end());
	result.insert(result.end(), v2.begin(), v2.end());
	(result.insert(result.end(), vectors.begin(), vectors.end()), ...);
	return result;
}

template <typename T>
static auto ConcatenateVectors(const std::vector<T>& v1, const std::vector<T>& v2) {
	std::vector<T> result;
	result.reserve(v1.size() + v2.size());
	result.insert(result.end(), v1.begin(), v1.end());
	result.insert(result.end(), v2.begin(), v2.end());
	return result;
}

} // namespace ptgn
