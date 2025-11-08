#pragma once

#include <array>
#include <functional>
#include <iosfwd>
#include <ostream>
#include <type_traits>

#include "core/util/concepts.h"
#include "debug/runtime/assert.h"
#include "math/math_utils.h"
#include "serialization/json/fwd.h"

// TODO: Stop exposing assert.h

namespace ptgn {

template <Arithmetic T>
struct Vector3 {
	T x{ 0 };
	T y{ 0 };
	T z{ 0 };

	constexpr T* Data() noexcept {
		static_assert(std::is_standard_layout_v<Vector3>);
		return &x;
	}

	constexpr const T* Data() const noexcept {
		static_assert(std::is_standard_layout_v<Vector3>);
		return &x;
	}

	constexpr Vector3() = default;

	template <Arithmetic U>
	explicit constexpr Vector3(U all) :
		x{ static_cast<T>(all) }, y{ static_cast<T>(all) }, z{ static_cast<T>(all) } {}

	constexpr Vector3(T x_component, T y_component, T z_component) :
		x{ x_component }, y{ y_component }, z{ z_component } {}

	explicit Vector3(const json& j);

	template <Arithmetic U>
	constexpr Vector3(const Vector3<U>& o) :
		x{ static_cast<T>(o.x) }, y{ static_cast<T>(o.y) }, z{ static_cast<T>(o.z) } {}

	template <ConvertibleToArithmetic U, ConvertibleToArithmetic S, ConvertibleToArithmetic V>
	constexpr Vector3(U x_component, S y_component, V z_component) :
		x{ static_cast<T>(x_component) },
		y{ static_cast<T>(y_component) },
		z{ static_cast<T>(z_component) } {}

	template <Arithmetic U>
	constexpr Vector3(const std::array<U, 3>& o) :
		x{ static_cast<T>(o[0]) }, y{ static_cast<T>(o[1]) }, z{ static_cast<T>(o[2]) } {}

	friend bool operator==(const Vector3& lhs, const Vector3& rhs) {
		return NearlyEqual(lhs.x, rhs.x) && NearlyEqual(lhs.y, rhs.y) && NearlyEqual(lhs.z, rhs.z);
	}

	// Access vector elements by index, 0 for x, 1 for y, 2 for z.
	[[nodiscard]] constexpr T& operator[](std::size_t idx) {
		if (idx == 1) {
			return y;
		} else if (idx == 2) {
			return z;
		}
		return x; // 0
	}

	// Access vector elements by index, 0 for x, 1 for y, 2 for z.
	[[nodiscard]] constexpr T operator[](std::size_t idx) const {
		if (idx == 1) {
			return y;
		} else if (idx == 2) {
			return z;
		}
		return x; // 0
	}

	[[nodiscard]] constexpr Vector3 operator-() const {
		return { -x, -y, -z };
	}

	template <Arithmetic U>
		requires NotNarrowingArithmetic<U, T>
	constexpr Vector3& operator+=(const Vector3<U>& rhs) {
		x += rhs.x;
		y += rhs.y;
		z += rhs.z;
		return *this;
	}

	template <Arithmetic U>
		requires NotNarrowingArithmetic<U, T>
	constexpr Vector3& operator-=(const Vector3<U>& rhs) {
		x -= rhs.x;
		y -= rhs.y;
		z -= rhs.z;
		return *this;
	}

	template <Arithmetic U>
		requires NotNarrowingArithmetic<U, T>
	constexpr Vector3& operator*=(const Vector3<U>& rhs) {
		x *= rhs.x;
		y *= rhs.y;
		z *= rhs.z;
		return *this;
	}

	template <Arithmetic U>
		requires NotNarrowingArithmetic<U, T>
	constexpr Vector3& operator/=(const Vector3<U>& rhs) {
		x /= rhs.x;
		y /= rhs.y;
		z /= rhs.z;
		return *this;
	}

	template <Arithmetic U>
		requires NotNarrowingArithmetic<U, T>
	constexpr Vector3& operator*=(U rhs) {
		x *= rhs;
		y *= rhs;
		z *= rhs;
		return *this;
	}

	template <Arithmetic U>
		requires NotNarrowingArithmetic<U, T>
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

	template <std::floating_point S = typename std::common_type_t<T, float>>
	[[nodiscard]] constexpr S Magnitude() const {
		return std::sqrt(static_cast<S>(MagnitudeSquared()));
	}

	[[nodiscard]] constexpr T MagnitudeSquared() const {
		return Dot(*this);
	}

	// Returns a unit vector (magnitude = 1) except for zero vectors (magnitude
	// = 0).
	template <std::floating_point S = typename std::common_type_t<T, float>>
	[[nodiscard]] Vector3<S> Normalized() const {
		T m{ MagnitudeSquared() };
		if (NearlyEqual(m, T{ 0 })) {
			return *this;
		}
		return *this / std::sqrt(static_cast<S>(m));
	}

	// See https://en.wikipedia.org/wiki/Rotation_matrix for details
	// Note: This is Euler angles and not Tait-Bryan angles.
	// Angles in radians.
	template <std::floating_point S = typename std::common_type_t<T, float>>
	[[nodiscard]] Vector3<S> Rotated(S yaw_radians, S pitch_radians, S roll_radians) const {
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

	// @return True if any component is zero.
	[[nodiscard]] bool HasZero() const {
		return NearlyEqual(x, T{ 0 }) || NearlyEqual(y, T{ 0 }) || NearlyEqual(z, T{ 0 });
	}
};

template <Arithmetic T>
void to_json(json& j, const Vector3<T>& vector);

template <Arithmetic T>
void from_json(const json& j, Vector3<T>& vector);

using V3_int	= Vector3<int>;
using V3_uint	= Vector3<unsigned int>;
using V3_float	= Vector3<float>;
using V3_double = Vector3<double>;

template <StreamWritable V>
inline std::ostream& operator<<(std::ostream& os, const ptgn::Vector3<V>& v) {
	os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
	return os;
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector3<S> operator+(const Vector3<V>& lhs, const Vector3<U>& rhs) {
	return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector3<S> operator-(const Vector3<V>& lhs, const Vector3<U>& rhs) {
	return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector3<S> operator*(const Vector3<V>& lhs, const Vector3<U>& rhs) {
	return { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector3<S> operator/(const Vector3<V>& lhs, const Vector3<U>& rhs) {
	return { lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector3<S> operator*(V lhs, const Vector3<U>& rhs) {
	return { lhs * rhs.x, lhs * rhs.y, lhs * rhs.z };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector3<S> operator*(const Vector3<V>& lhs, U rhs) {
	return { lhs.x * rhs, lhs.y * rhs, lhs.z * rhs };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector3<S> operator/(V lhs, const Vector3<U>& rhs) {
	return { lhs / rhs.x, lhs / rhs.y, lhs / rhs.z };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector3<S> operator/(const Vector3<V>& lhs, U rhs) {
	return { lhs.x / rhs, lhs.y / rhs, lhs.z / rhs };
}

} // namespace ptgn

// Custom hashing function for Vector3 class.
// This allows for use of unordered maps and sets with Vector2s as keys.
template <ptgn::Arithmetic T>
struct std::hash<ptgn::Vector3<T>> {
	std::size_t operator()(const ptgn::Vector3<T>& v) const noexcept {
		// Hashing combination algorithm from:
		// https://stackoverflow.com/a/17017281
		std::size_t value{ 17 };
		value = value * 31 + std::hash<T>()(v.x);
		value = value * 31 + std::hash<T>()(v.y);
		value = value * 31 + std::hash<T>()(v.z);
		return value;
	}
};