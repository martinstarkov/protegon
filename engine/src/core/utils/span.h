#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <map>
#include <memory>
#include <ranges>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/utils/concepts.h"

// TODO: Get rid of stuff that is outdated as of my C++ 20 move.

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

// @return How many bits the contents of the vector take up.
template <typename T>
inline std::size_t Sizeof(const std::vector<T>& vector) {
	return sizeof(T) * vector.size();
}

// @return How many bits the contents of the array take up.
template <typename T, std::size_t I>
inline constexpr std::size_t Sizeof(const std::array<T, I>& array) {
	return sizeof(T) * array.size();
}

template <typename T>
inline std::vector<T> ToVector(const std::unordered_set<T>& set) {
	std::vector<T> v;
	v.reserve(set.size());
	for (const auto& element : set) {
		v.emplace_back(element);
	}
	return v;
}

template <typename Type, std::size_t Size>
inline std::vector<Type> ToVector(const std::array<Type, Size>& array) {
	std::vector<Type> v;
	v.reserve(Size);
	for (const auto& a : array) {
		v.emplace_back(a);
	}
	return v;
}

template <typename Type>
inline const std::vector<Type>& ToVector(const std::vector<Type>& vector) {
	return vector;
}

template <typename Key, typename Value, typename Hash, typename Pred, typename Alloc>
[[nodiscard]] inline auto GetKeys(const std::unordered_map<Key, Value, Hash, Pred, Alloc>& map) {
	return impl::GetElements<Key>(map);
}

template <typename Key, typename Value, typename Hash, typename Pred, typename Alloc>
[[nodiscard]] inline auto GetValues(const std::unordered_map<Key, Value, Hash, Pred, Alloc>& map) {
	return impl::GetElements<Value>(map);
}

template <typename Key, typename Value, typename Compare, typename Alloc>
[[nodiscard]] inline auto GetKeys(const std::map<Key, Value, Compare, Alloc>& map) {
	return impl::GetElements<Key>(map);
}

template <typename Key, typename Value, typename Compare, typename Alloc>
[[nodiscard]] inline auto GetValues(const std::map<Key, Value, Compare, Alloc>& map) {
	return impl::GetElements<Value>(map);
}

template <typename T>
[[nodiscard]] inline bool VectorContains(const std::vector<T>& container, const T& value) {
	return std::ranges::find(container, value) != container.end();
}

template <typename T, typename Predicate>
[[nodiscard]] inline bool VectorFindIf(const std::vector<T>& container, Predicate&& condition) {
	return std::ranges::find_if(container, std::forward<Predicate>(condition)) != container.end();
}

template <typename Type, std::size_t... sizes>
[[nodiscard]] inline auto ConcatenateArrays(const std::array<Type, sizes>&... arrays) {
	std::array<Type, (sizes + ...)> result;
	std::size_t index{ 0 };

	((std::copy_n(arrays.begin(), sizes, result.begin() + index), index += sizes), ...);

	return result;
}

template <typename T, typename... TArgs>
[[nodiscard]] inline auto ConcatenateVectors(
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
[[nodiscard]] inline auto ConcatenateVectors(const std::vector<T>& v1, const std::vector<T>& v2) {
	std::vector<T> result;
	result.reserve(v1.size() + v2.size());
	result.insert(result.end(), v1.begin(), v1.end());
	result.insert(result.end(), v2.begin(), v2.end());
	return result;
}

template <typename T>
void VectorRemoveDuplicates(std::vector<T>& v) {
	std::sort(v.begin(), v.end());
	auto last{ std::ranges::unique(v) };
	v.erase(last.begin(), last.end());
}

// Swaps vector elements if they both exist in the vector.
template <typename T>
inline void VectorSwapElements(std::vector<T>& v, const T& e1, const T& e2) {
	auto it1{ std::find(v.begin(), v.end(), e1) };
	auto it2{ std::find(v.begin(), v.end(), e2) };
	if (it1 == v.end() || it2 == v.end()) {
		return;
	}
	std::swap(*it1, *it2);
}

// Do not emplace if condition is true.
// @return first True if emplaced, false if condition was met
//         second Reference to emplaced or existing element.
template <typename T, typename Predicate, typename... Args>
inline std::pair<bool, T&> VectorTryEmplaceIf(
	std::vector<T>& vec, Predicate&& condition, Args&&... args
) {
	for (auto& item : vec) {
		if (condition(item)) {
			return { false, item };
		}
	}
	return { true, vec.emplace_back(std::forward<Args>(args)...) };
}

// Do not emplace if condition is true.
// @return first True if emplaced, false if condition was met
//         second Reference to emplaced or existing element.
template <typename S, typename T, typename Predicate, typename... Args>
	requires IsOrDerivedFrom<S, T>
inline std::pair<bool, T&> VectorTryEmplaceIf(
	std::vector<std::shared_ptr<T>>& vec, Predicate&& condition, Args&&... args
) {
	for (const auto& item : vec) {
		if (condition(item)) {
			return { false, item };
		}
	}
	return { true, vec.emplace_back(std::make_shared<S>(std::forward<Args>(args)...)) };
}

// @return first True if replaced, false if emplaced.
//         second Reference to replaced or emplaced element.
template <typename T, typename Predicate, typename... Args>
inline std::pair<bool, T&> VectorReplaceOrEmplaceIf(
	std::vector<T>& vec, Predicate&& condition, Args&&... args
) {
	for (auto& item : vec) {
		if (condition(item)) {
			item = T{ std::forward<Args>(args)... };
			return { true, item }; // Replaced.
		}
	}
	return { false, vec.emplace_back(std::forward<Args>(args)...) }; // Emplaced.
}

// @return first True if replaced, false if emplaced.
//         second Reference to replaced or emplaced element.
template <typename S, typename T, typename Predicate, typename... Args>
	requires IsOrDerivedFrom<S, T>
inline std::pair<bool, std::shared_ptr<T>&> VectorReplaceOrEmplaceIf(
	std::vector<std::shared_ptr<T>>& vec, Predicate&& condition, Args&&... args
) {
	for (auto& item : vec) {
		if (condition(item)) {
			item = std::make_shared<S>(std::forward<Args>(args)...);
			return { true, item }; // Replaced.
		}
	}
	return { false,
			 vec.emplace_back(std::make_shared<S>(std::forward<Args>(args)...)) }; // Emplaced.
}

// @return True if the element was erased from the vector, false otherwise.
template <typename T, typename Predicate>
inline bool VectorEraseIf(std::vector<T>& v, Predicate&& condition) {
	auto before{ v.size() };
	std::erase_if(v, condition);
	return v.size() != before;
}

// @return True if the element was erased from the vector, false otherwise.
template <typename T>
inline bool VectorErase(std::vector<T>& v, const T& element) {
	auto before{ v.size() };
	std::erase(v, element);
	return v.size() != before;
}

// Subtract elements of b from a.
template <typename T>
inline void VectorSubtract(std::vector<T>& a, const std::vector<T>& b) {
	// Create a hash set of elements in b for fast lookup
	std::unordered_set<T> b_set(b.begin(), b.end());

	// Erase all elements from a that are in b_set
	std::erase_if(a, [&b_set](const T& val) { return b_set.count(val) > 0; });
}

} // namespace ptgn
