#pragma once

#include <iosfwd>
#include <ostream>
#include <type_traits>

#include "utility/debug.h"
#include "utility/type_traits.h"

namespace ptgn {

template <typename T, tt::arithmetic<T> = true>
struct Vector3 {
	T x{ 0 };
	T y{ 0 };
	T z{ 0 };

	constexpr Vector3()					   = default;
	~Vector3()							   = default;
	Vector3(const Vector3&)				   = default;
	Vector3(Vector3&&) noexcept			   = default;
	Vector3& operator=(const Vector3&)	   = default;
	Vector3& operator=(Vector3&&) noexcept = default;

	explicit constexpr Vector3(T all) : x{ all }, y{ all }, z{ all } {}

	constexpr Vector3(T x, T y, T z) : x{ x }, y{ y }, z{ z } {}

	// TODO: Check that not_narrowing actually works as intended and static cast is not narrowing.
	template <typename U, tt::not_narrowing<U, T> = true>
	constexpr Vector3(const Vector3<U>& o) :
		x{ static_cast<T>(o.x) }, y{ static_cast<T>(o.y) }, z{ static_cast<T>(o.z) } {}

	// Note: use of explicit keyword for narrowing constructors.

	template <typename U, tt::narrowing<U, T> = true>
	explicit constexpr Vector3(U x, U y, U z) :
		x{ static_cast<T>(x) }, y{ static_cast<T>(y) }, z{ static_cast<T>(z) } {}

	template <typename U, tt::narrowing<U, T> = true>
	explicit constexpr Vector3(const Vector3<U>& o) :
		x{ static_cast<T>(o.x) }, y{ static_cast<T>(o.y) }, z{ static_cast<T>(o.z) } {}

	// Access vector elements by index, 0 for x, 1 for y, 2 for z.
	[[nodiscard]] constexpr T& operator[](std::size_t idx) {
		PTGN_ASSERT(idx >= 0 && idx < 3, "Vector3 subscript out of range");
		if (idx == 0) {
			return x;
		} else if (idx == 1) {
			return y;
		}
		return z; // idx == 2
	}

	// Access vector elements by index, 0 for x, 1 for y, 2 for z.
	[[nodiscard]] constexpr T operator[](std::size_t idx) const {
		PTGN_ASSERT(idx >= 0 && idx < 3, "Vector3 subscript out of range");
		if (idx == 0) {
			return x;
		} else if (idx == 1) {
			return y;
		}
		return z; // idx == 2
	}

	[[nodiscard]] constexpr Vector3 operator-() const {
		return { -x, -y, -z };
	}

	template <typename U, tt::not_narrowing<U, T> = true>
	constexpr Vector3& operator+=(const Vector3<U>& rhs) {
		x += rhs.x;
		y += rhs.y;
		z += rhs.z;
		return *this;
	}

	template <typename U, tt::not_narrowing<U, T> = true>
	constexpr Vector3& operator-=(const Vector3<U>& rhs) {
		x -= rhs.x;
		y -= rhs.y;
		z -= rhs.z;
		return *this;
	}

	template <typename U, tt::not_narrowing<U, T> = true>
	constexpr Vector3& operator*=(const Vector3<U>& rhs) {
		x *= rhs.x;
		y *= rhs.y;
		z *= rhs.z;
		return *this;
	}

	template <typename U, tt::not_narrowing<U, T> = true>
	constexpr Vector3& operator/=(const Vector3<U>& rhs) {
		x /= rhs.x;
		y /= rhs.y;
		z /= rhs.z;
		return *this;
	}

	template <typename U, tt::not_narrowing<U, T> = true>
	constexpr Vector3& operator*=(U rhs) {
		x *= rhs;
		y *= rhs;
		z *= rhs;
		return *this;
	}

	template <typename U, tt::not_narrowing<U, T> = true>
	constexpr Vector3& operator/=(U rhs) {
		x /= rhs;
		y /= rhs;
		z /= rhs;
		return *this;
	}

	// Returns the dot product (this * o).
	[[nodiscard]] constexpr T Dot(const Vector3& o) const {
		return x * o.x + y * o.y + z * o.z;
	}

	// Returns the cross product (this x o).
	[[nodiscard]] constexpr Vector3 Cross(const Vector3& o) const {
		return { y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.z };
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
	[[nodiscard]] Vector3<S> Normalized() const {
		static_assert(std::is_floating_point_v<S>, "Function requires floating point type");
		T m{ MagnitudeSquared() };
		if (NearlyEqual(m, T{ 0 })) {
			return *this;
		}
		return *this / std::sqrt(static_cast<S>(m));
	}

	// See https://en.wikipedia.org/wiki/Rotation_matrix for details
	// Note: This is Euler angles and not Tait-Bryan angles.
	// Angles in radians.
	template <typename S = typename std::common_type_t<T, float>>
	[[nodiscard]] Vector3<S> Rotated(S yaw_radians, S pitch_radians, S roll_radians) const {
		static_assert(std::is_floating_point_v<S>, "Function requires floating point type");
		auto sin_a = std::sin(yaw_radians);
		auto cos_a = std::cos(yaw_radians);
		auto sin_B = std::sin(pitch_radians);
		auto cos_B = std::cos(pitch_radians);
		auto sin_y = std::sin(roll_radians);
		auto cos_y = std::cos(roll_radians);
		return { x * (cos_B * cos_y) + y * (sin_a * sin_B * cos_y - cos_a * sin_y) +
					 z * (cos_a * sin_B * cos_y + sin_a * sin_y),
				 x * (cos_B * sin_y) + y * (sin_a * sin_B * sin_y + cos_a * cos_y) +
					 z * (cos_a * sin_B * sin_y - sin_a * cos_y),
				 x * (-sin_B) + y * (sin_a * cos_B) + z * (cos_a * cos_B) };
	}

	[[nodiscard]] bool IsZero() const {
		return NearlyEqual(x, T{ 0 }) && NearlyEqual(y, T{ 0 }) && NearlyEqual(z, T{ 0 });
	}
};

using V3_int	= Vector3<int>;
using V3_uint	= Vector3<unsigned int>;
using V3_float	= Vector3<float>;
using V3_double = Vector3<double>;

template <typename V>
[[nodiscard]] inline bool operator==(const Vector3<V>& lhs, const Vector3<V>& rhs) {
	return NearlyEqual(lhs.x, rhs.x) && NearlyEqual(lhs.y, rhs.y) && NearlyEqual(lhs.z, rhs.z);
}

template <typename V>
[[nodiscard]] inline bool operator!=(const Vector3<V>& lhs, const Vector3<V>& rhs) {
	return !operator==(lhs, rhs);
}

template <typename V, ptgn::tt::stream_writable<std::ostream, V> = true>
inline std::ostream& operator<<(std::ostream& os, const ptgn::Vector3<V>& v) {
	os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
	return os;
}

template <typename V, typename U, typename S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector3<S> operator+(const Vector3<V>& lhs, const Vector3<U>& rhs) {
	return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
}

template <typename V, typename U, typename S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector3<S> operator-(const Vector3<V>& lhs, const Vector3<U>& rhs) {
	return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
}

template <typename V, typename U, typename S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector3<S> operator*(const Vector3<V>& lhs, const Vector3<U>& rhs) {
	return { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z };
}

template <typename V, typename U, typename S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector3<S> operator/(const Vector3<V>& lhs, const Vector3<U>& rhs) {
	return { lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z };
}

template <
	typename V, typename U, tt::arithmetic<V> = true,
	typename S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector3<S> operator*(V lhs, const Vector3<U>& rhs) {
	return { lhs * rhs.x, lhs * rhs.y, lhs * rhs.z };
}

template <
	typename V, typename U, tt::arithmetic<U> = true,
	typename S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector3<S> operator*(const Vector3<V>& lhs, U rhs) {
	return { lhs.x * rhs, lhs.y * rhs, lhs.z * rhs };
}

template <
	typename V, typename U, tt::arithmetic<V> = true,
	typename S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector3<S> operator/(V lhs, const Vector3<U>& rhs) {
	return { lhs / rhs.x, lhs / rhs.y, lhs / rhs.z };
}

template <
	typename V, typename U, tt::arithmetic<U> = true,
	typename S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector3<S> operator/(const Vector3<V>& lhs, U rhs) {
	return { lhs.x / rhs, lhs.y / rhs, lhs.z / rhs };
}

} // namespace ptgn

// Custom hashing function for Vector3 class.
// This allows for use of unordered maps and sets with Vector2s as keys.
template <typename T>
struct std::hash<ptgn::Vector3<T>> {
	std::size_t operator()(const ptgn::Vector3<T>& v) const noexcept {
		// Hashing combination algorithm from:
		// https://stackoverflow.com/a/17017281
		std::size_t hash{ 17 };
		hash = hash * 31 + std::hash<T>()(v.x);
		hash = hash * 31 + std::hash<T>()(v.y);
		hash = hash * 31 + std::hash<T>()(v.z);
		return hash;
	}
};