#pragma once

#include <array>
#include <map>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ptgn {

namespace impl {

template <typename ReturnType, typename Map>
std::vector<ReturnType> GetElements(const Map& map) {
	using K = typename Map::key_type;
	using V = typename Map::mapped_type;

	static_assert(std::is_same_v<ReturnType, K> || std::is_same_v<ReturnType, V>);

	std::vector<ReturnType> elements;
	elements.reserve(map.size());
	for (const auto& [key, value] : map) {
		if constexpr (std::is_same_v<ReturnType, K>) {
			elements.emplace_back(key);
		} else if constexpr (std::is_same_v<ReturnType, V>) {
			elements.emplace_back(value);
		}
	}
	return elements;
}

} // namespace impl

template <typename Key, typename Value, typename Hash, typename Pred, typename Alloc>
[[nodiscard]] static auto GetKeys(const std::unordered_map<Key, Value, Hash, Pred, Alloc>& map) {
	return impl::GetElements<Key>(map);
}

template <typename Key, typename Value, typename Hash, typename Pred, typename Alloc>
[[nodiscard]] static auto GetValues(const std::unordered_map<Key, Value, Hash, Pred, Alloc>& map) {
	return impl::GetElements<Value>(map);
}

template <typename Key, typename Value, typename Compare, typename Alloc>
[[nodiscard]] static auto GetKeys(const std::map<Key, Value, Compare, Alloc>& map) {
	return impl::GetElements<Key>(map);
}

template <typename Key, typename Value, typename Compare, typename Alloc>
[[nodiscard]] static auto GetValues(const std::map<Key, Value, Compare, Alloc>& map) {
	return impl::GetElements<Value>(map);
}

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

// Swaps vector elements if they both exist in the vector.
template <typename T>
static void SwapVectorElements(std::vector<T>& v, const T& e1, const T& e2) {
	auto it1{ std::find(v.begin(), v.end(), e1) };
	auto it2{ std::find(v.begin(), v.end(), e2) };
	if (it1 == v.end() || it2 == v.end()) {
		return;
	}
	std::swap(*it1, *it2);
}

} // namespace ptgn
