#pragma once

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <ostream>

#include "protegon/math.h"
#include "utility/debug.h"
#include "utility/type_traits.h"

namespace ptgn {

template <typename T, tt::arithmetic<T> = true>
struct Vector4 {
	T x{ 0 };
	T y{ 0 };
	T z{ 0 };
	T w{ 0 };

	constexpr Vector4()				   = default;
	~Vector4()						   = default;
	Vector4(const Vector4&)			   = default;
	Vector4(Vector4&&)				   = default;
	Vector4& operator=(const Vector4&) = default;
	Vector4& operator=(Vector4&&)	   = default;

	explicit constexpr Vector4(T all) : x{ all }, y{ all }, z{ all }, w{ all } {}

	constexpr Vector4(T x, T y, T z, T w) : x{ x }, y{ y }, z{ z }, w{ w } {}

	// TODO: Check that not_narrowing actually works as intended and static cast is not narrowing.
	template <typename U, tt::not_narrowing<U, T> = true>
	constexpr Vector4(const Vector4<U>& o) :
		x{ static_cast<T>(o.x) },
		y{ static_cast<T>(o.y) },
		z{ static_cast<T>(o.z) },
		w{ static_cast<T>(o.w) } {}

	// Note: use of explicit keyword for narrowing constructors.

	template <typename U, tt::narrowing<U, T> = true>
	explicit constexpr Vector4(U x, U y, U z, U w) :
		x{ static_cast<T>(x) },
		y{ static_cast<T>(y) },
		z{ static_cast<T>(z) },
		w{ static_cast<T>(w) } {}

	template <typename U, tt::narrowing<U, T> = true>
	explicit constexpr Vector4(const Vector4<U>& o) :
		x{ static_cast<T>(o.x) },
		y{ static_cast<T>(o.y) },
		z{ static_cast<T>(o.z) },
		w{ static_cast<T>(o.w) } {}

	// Access vector elements by index, 0 for x, 1 for y, 2 for z, 3 for w.
	constexpr T& operator[](std::size_t idx) {
		PTGN_ASSERT(idx >= 0 && idx < 4, "Vector4 subscript out of range");
		if (idx == 0) {
			return x;
		} else if (idx == 1) {
			return y;
		} else if (idx == 2) {
			return z;
		}
		return w; // idx == 3
	}

	// Access vector elements by index, 0 for x, 1 for y, 2 for z, 3 for w.
	constexpr T operator[](std::size_t idx) const {
		PTGN_ASSERT(idx >= 0 && idx < 4, "Vector4 subscript out of range");
		if (idx == 0) {
			return x;
		} else if (idx == 1) {
			return y;
		} else if (idx == 2) {
			return z;
		}
		return w; // idx == 3
	}

	constexpr Vector4 operator-() const {
		return { -x, -y, -z, -w };
	}

	template <typename U, tt::not_narrowing<U, T> = true>
	constexpr Vector4& operator+=(const Vector4<U>& rhs) {
		x += rhs.x;
		y += rhs.y;
		z += rhs.z;
		w += rhs.w;
		return *this;
	}

	template <typename U, tt::not_narrowing<U, T> = true>
	constexpr Vector4& operator-=(const Vector4<U>& rhs) {
		x -= rhs.x;
		y -= rhs.y;
		z -= rhs.z;
		w -= rhs.w;
		return *this;
	}

	template <typename U, tt::not_narrowing<U, T> = true>
	constexpr Vector4& operator*=(const Vector4<U>& rhs) {
		x *= rhs.x;
		y *= rhs.y;
		z *= rhs.z;
		w *= rhs.w;
		return *this;
	}

	template <typename U, tt::not_narrowing<U, T> = true>
	constexpr Vector4& operator/=(const Vector4<U>& rhs) {
		x /= rhs.x;
		y /= rhs.y;
		z /= rhs.z;
		w /= rhs.w;
		return *this;
	}

	template <typename U, tt::not_narrowing<U, T> = true>
	constexpr Vector4& operator*=(U rhs) {
		x *= rhs;
		y *= rhs;
		z *= rhs;
		w *= rhs;
		return *this;
	}

	template <typename U, tt::not_narrowing<U, T> = true>
	constexpr Vector4& operator/=(U rhs) {
		x /= rhs;
		y /= rhs;
		z /= rhs;
		w /= rhs;
		return *this;
	}

	// Returns the dot product (this * o).
	[[nodiscard]] constexpr T Dot(const Vector4& o) const {
		return x * o.x + y * o.y + z * o.z + w * o.w;
	}

	template <typename S = typename std::common_type_t<T, float>>
	[[nodiscard]] constexpr S Magnitude() const {
		static_assert(std::is_floating_point_v<S>, "Function requires floating point type");
		return std::sqrt(static_cast<S>(MagnitudeSquared()));
	}

	[[nodiscard]] constexpr T MagnitudeSquared() const {
		return Dot(*this);
	}

	// Returns a unit vector (magnitude = 1) except for zero vectors (magnitude
	// = 0).
	template <typename S = typename std::common_type_t<T, float>>
	[[nodiscard]] Vector4<S> Normalized() const {
		static_assert(std::is_floating_point_v<S>, "Function requires floating point type");
		T m{ MagnitudeSquared() };
		if (NearlyEqual(m, T{ 0 })) {
			return *this;
		}
		return *this / std::sqrt(static_cast<S>(m));
	}

	[[nodiscard]] bool IsZero() const {
		return NearlyEqual(x, T{ 0 }) && NearlyEqual(y, T{ 0 }) && NearlyEqual(z, T{ 0 }) &&
			   NearlyEqual(w, T{ 0 });
	}
};

using V4_int	= Vector4<int>;
using V4_uint	= Vector4<unsigned int>;
using V4_float	= Vector4<float>;
using V4_double = Vector4<double>;

template <typename T>
inline bool operator==(const Vector4<T>& lhs, const Vector4<T>& rhs) {
	return NearlyEqual(lhs.x, rhs.x) && NearlyEqual(lhs.y, rhs.y) && NearlyEqual(lhs.z, rhs.z) &&
		   NearlyEqual(lhs.w, rhs.w);
}

template <typename T>
inline bool operator!=(const Vector4<T>& lhs, const Vector4<T>& rhs) {
	return !operator==(lhs, rhs);
}

template <typename T, typename U, typename S = typename std::common_type_t<T, U>>
constexpr inline Vector4<S> operator+(const Vector4<T>& lhs, const Vector4<U>& rhs) {
	return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w };
}

template <typename T, typename U, typename S = typename std::common_type_t<T, U>>
constexpr inline Vector4<S> operator-(const Vector4<T>& lhs, const Vector4<U>& rhs) {
	return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w };
}

template <typename T, typename U, typename S = typename std::common_type_t<T, U>>
constexpr inline Vector4<S> operator*(const Vector4<T>& lhs, const Vector4<U>& rhs) {
	return { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w };
}

template <typename T, typename U, typename S = typename std::common_type_t<T, U>>
constexpr inline Vector4<S> operator/(const Vector4<T>& lhs, const Vector4<U>& rhs) {
	return { lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z, lhs.w / rhs.w };
}

template <
	typename T, typename U, tt::arithmetic<T> = true,
	typename S = typename std::common_type_t<T, U>>
constexpr inline Vector4<S> operator*(T lhs, const Vector4<U>& rhs) {
	return { lhs * rhs.x, lhs * rhs.y, lhs * rhs.z, lhs * rhs.w };
}

template <
	typename T, typename U, tt::arithmetic<U> = true,
	typename S = typename std::common_type_t<T, U>>
constexpr inline Vector4<S> operator*(const Vector4<T>& lhs, U rhs) {
	return { lhs.x * rhs, lhs.y * rhs, lhs.z * rhs, lhs.w * rhs };
}

template <
	typename T, typename U, tt::arithmetic<T> = true,
	typename S = typename std::common_type_t<T, U>>
constexpr inline Vector4<S> operator/(T lhs, const Vector4<U>& rhs) {
	return { lhs / rhs.x, lhs / rhs.y, lhs / rhs.z, lhs / rhs.w };
}

template <
	typename T, typename U, tt::arithmetic<T> = true,
	typename S = typename std::common_type_t<T, U>>
constexpr inline Vector4<S> operator/(const Vector4<T>& lhs, U rhs) {
	return { lhs.x / rhs, lhs.y / rhs, lhs.z / rhs, lhs.w / rhs };
}

template <typename T, ptgn::tt::stream_writable<std::ostream, T> = true>
inline std::ostream& operator<<(std::ostream& os, const ptgn::Vector4<T>& v) {
	os << "(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")";
	return os;
}

} // namespace ptgn

// Custom hashing function for Vector4 class.
// This allows for use of unordered maps and sets with Vector2s as keys.
template <typename T>
struct std::hash<ptgn::Vector4<T>> {
	std::size_t operator()(const ptgn::Vector4<T>& v) const noexcept {
		// Hashing combination algorithm from:
		// https://stackoverflow.com/a/17017281
		std::size_t hash{ 17 };
		hash = hash * 31 + std::hash<T>()(v.x);
		hash = hash * 31 + std::hash<T>()(v.y);
		hash = hash * 31 + std::hash<T>()(v.z);
		hash = hash * 31 + std::hash<T>()(v.w);
		return hash;
	}
};