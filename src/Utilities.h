#pragma once

#include <iomanip> // setprecision
#include <sstream> // string streams for limiting precision / significant figures
#include <map>
#include <vector>
#include <set>
#include <cmath>
#include <cassert>

#include "AllocationMetrics.h"

struct Vec2D;
struct AABB;
struct SDL_Rect;

namespace Util {
	// https://stackoverflow.com/a/29634934 Credit to Jarod42
	namespace detail {
		// To allow ADL with custom begin/end
		using std::begin;
		using std::end;

		template <typename T>
		auto is_iterable_impl(int)
			-> decltype (
				begin(std::declval<T&>()) != end(std::declval<T&>()), // begin/end and operator !=
				void(), // Handle evil operator ,
				++std::declval<decltype(begin(std::declval<T&>()))&>(), // operator ++
				void(*begin(std::declval<T&>())), // operator*
				std::true_type{});

		template <typename T>
		std::false_type is_iterable_impl(...);

		// https://stackoverflow.com/a/35293958 // Credit to Jonathan Wakely
		// Needed for some older versions of GCC
		template<typename...>
		struct voider { using type = void; };

		// std::void_t will be part of C++17, but until then define it ourselves:
		template<typename... T>
		using void_t = typename voider<T...>::type;

		template<typename T, typename U = void>
		struct is_mappish_impl : std::false_type {};

		template<typename T>
		struct is_mappish_impl<T, void_t<typename T::key_type,
			typename T::mapped_type,
			decltype(std::declval<T&>()[std::declval<const typename T::key_type&>()])>>
			: std::true_type { };
	}

	template <typename T>
	// Find if type is iterable
	using is_iterable = decltype(detail::is_iterable_impl<T>(0));

	template<typename T>
	struct is_mappish : detail::is_mappish_impl<T>::type {};

	template<typename T>
	// is not pointer
	struct is_pointer { static const bool value = false; };

	template<typename T>
	// is pointer
	struct is_pointer<T*> { static const bool value = true; };

	// Truncate to specific amount of significant figures
	double truncate(double value, int digits);

	// Find the sign of a numeric type
	template <typename T>
	inline int sgn(T val) {
		static_assert(std::is_arithmetic<T>::value, "sgn can only accept numeric types");
		return (T(0) < val) - (val < T(0));
	}

	template<class T>
	constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
		static_assert(std::is_arithmetic<T>::value, "clamp can only accept numeric types");
		assert(!(hi < lo));
		return (v < lo) ? lo : (hi < v) ? hi : v;
	}


	// Call functions on variadic templates
	template <typename ...Ts>
	inline void swallow(Ts&&... args) {}

	// Return an SDL_Rect from position and size vectors
	SDL_Rect RectFromVec(const Vec2D& position, const Vec2D& size);

	// Return an SDL_Rect from AABB
	SDL_Rect RectFromAABB(const AABB& aabb);

	// Delete a set of indexes from a vector or set
	template <typename T, typename S>
	static void eraseSetFromSet(const std::set<T>& eraseThis, std::set<S>& fromThis) {
		static_assert(std::is_arithmetic<T>::value, "Cannot erase non-numeric indexes from a set");
		std::size_t iteratorOffset = 0;
		for (auto& index : eraseThis) {
			auto it = std::begin(fromThis);
			std::advance(it, index - iteratorOffset);
			fromThis.erase(it);
			iteratorOffset++;
		}
	}

	template <typename T>
	static std::ostream& printIterable(std::ostream& os, const T& iterable) {
		os << "[ ";
		for (auto it = std::begin(iterable); it != std::end(iterable); ++it) {
			if (is_iterable<decltype(*it)>::value) {
				printIterable(os, *it);
			} else {
				os << *it << " ";
			}
		}
		os << "]";
		return os;
	}

	template <typename K, typename V>
	static std::ostream& printMap(std::ostream& os, const std::map<K, V>& map) {
		os << "{ " << std::endl;
		bool isIterable = is_iterable<V>::value;
		bool isMap = is_mappish<V>{};
		bool isString = std::is_same<V, std::string>::value;
		bool isPointer = is_pointer<V>::value;
		for (auto& pair : map) {
			os << "[ " << pair.first << ", ";
			os << pair.second;
			os << " ]" << std::endl;
		}
		os << "}";
		return os;
	}

}

// Operator overloading

template <typename T>
std::ostream& operator<< (std::ostream& os, const std::vector<T>& vector) {
	return Util::printIterable(os, vector);
}

template <typename T>
std::ostream& operator<< (std::ostream& os, const std::set<T>& set) {
	return Util::printIterable(os, set);
}

template <typename V, typename K>
std::ostream& operator<< (std::ostream& os, const std::map<K, V>& map) {
	return Util::printMap(os, map);
}