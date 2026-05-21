#pragma once

#include <array>
#include <cmath>
#include <functional>
#include <ostream>
#include <type_traits>

#include "core/math/angle.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "core/util/hash.h"
#include "serialization/json/fwd.h"

namespace ptgn {

template <Arithmetic T>
struct Vector3 {
	T x{ 0 };
	T y{ 0 };
	T z{ 0 };

	[[nodiscard]] constexpr T* Data() noexcept {
		static_assert(std::is_standard_layout_v<Vector3>);
		return &x;
	}

	[[nodiscard]] constexpr const T* Data() const noexcept {
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
	constexpr Vector3(Vector3<U> o) : // NOSONAR
		x{ static_cast<T>(o.x) }, y{ static_cast<T>(o.y) }, z{ static_cast<T>(o.z) } {}

	template <ConvertibleToArithmetic U, ConvertibleToArithmetic S, ConvertibleToArithmetic V>
	constexpr Vector3(U x_component, S y_component, V z_component) :
		x{ static_cast<T>(x_component) },
		y{ static_cast<T>(y_component) },
		z{ static_cast<T>(z_component) } {}

	template <Arithmetic U>
	explicit constexpr Vector3(std::array<U, 3> o) :
		x{ static_cast<T>(o[0]) }, y{ static_cast<T>(o[1]) }, z{ static_cast<T>(o[2]) } {}

	[[nodiscard]] constexpr Vector2<T> xy() const {
		return { x, y };
	}

	[[nodiscard]] constexpr Vector2<T> xx() const {
		return { x, x };
	}

	[[nodiscard]] constexpr Vector2<T> yy() const {
		return { y, y };
	}

	[[nodiscard]] constexpr Vector2<T> zz() const {
		return { z, z };
	}

	friend bool operator==(const Vector3& lhs, const Vector3& rhs) {
		return NearlyEqual(lhs.x, rhs.x) && NearlyEqual(lhs.y, rhs.y) && NearlyEqual(lhs.z, rhs.z);
	}

	/// @brief Access vector elements by index, 0 for x, 1 for y, 2 for z.
	constexpr T& operator[](std::size_t idx) {
		if (idx == 1) {
			return y;
		} else if (idx == 2) {
			return z;
		}
		return x; // 0
	}

	/// @brief Access vector elements by index, 0 for x, 1 for y, 2 for z.
	constexpr T operator[](std::size_t idx) const {
		if (idx == 1) {
			return y;
		} else if (idx == 2) {
			return z;
		}
		return x; // 0
	}

	constexpr Vector3 operator-() const {
		return { -x, -y, -z };
	}

	template <Arithmetic U>
		requires NotNarrowingArithmetic<U, T>
	constexpr Vector3& operator+=(Vector3<U> rhs) {
		x += rhs.x;
		y += rhs.y;
		z += rhs.z;
		return *this;
	}

	template <Arithmetic U>
		requires NotNarrowingArithmetic<U, T>
	constexpr Vector3& operator-=(Vector3<U> rhs) {
		x -= rhs.x;
		y -= rhs.y;
		z -= rhs.z;
		return *this;
	}

	template <Arithmetic U>
		requires NotNarrowingArithmetic<U, T>
	constexpr Vector3& operator*=(Vector3<U> rhs) {
		x *= rhs.x;
		y *= rhs.y;
		z *= rhs.z;
		return *this;
	}

	template <Arithmetic U>
		requires NotNarrowingArithmetic<U, T>
	constexpr Vector3& operator/=(Vector3<U> rhs) {
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

	/// @return The dot product (this * o).
	[[nodiscard]] constexpr T Dot(Vector3 o) const {
		return x * o.x + y * o.y + z * o.z;
	}

	/// @return The cross product (this x o).
	[[nodiscard]] constexpr Vector3 Cross(Vector3 o) const {
		return { y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.z };
	}

	[[nodiscard]] constexpr float Magnitude() const {
		return std::sqrt(static_cast<float>(MagnitudeSquared()));
	}

	[[nodiscard]] constexpr T MagnitudeSquared() const {
		return Dot(*this);
	}

	/// @return Unit vector (magnitude = 1) except for zero vectors (magnitude = 0).
	[[nodiscard]] Vector3<float> Normalized() const {
		T m{ MagnitudeSquared() };
		if (NearlyEqual(m, T{ 0 })) {
			return *this;
		}
		return *this / std::sqrt(static_cast<float>(m));
	}

	/// @brief See https://en.wikipedia.org/wiki/Rotation_matrix for details
	/// Note: This is Euler angles and not Tait-Bryan angles.
	[[nodiscard]] Vector3<float> Rotated(Radians yaw, Radians pitch, Radians roll) const {
		auto sin_a = yaw.Sin();
		auto cos_a = yaw.Cos();
		auto sin_B = pitch.Sin();
		auto cos_B = pitch.Cos();
		auto sin_y = roll.Sin();
		auto cos_y = roll.Cos();
		return { x * (cos_B * cos_y) + y * (sin_a * sin_B * cos_y - cos_a * sin_y) +
					 z * (cos_a * sin_B * cos_y + sin_a * sin_y),
				 x * (cos_B * sin_y) + y * (sin_a * sin_B * sin_y + cos_a * cos_y) +
					 z * (cos_a * sin_B * sin_y - sin_a * cos_y),
				 x * (-sin_B) + y * (sin_a * cos_B) + z * (cos_a * cos_B) };
	}

	/// @brief See https://en.wikipedia.org/wiki/Rotation_matrix for details
	/// Note: This is Euler angles and not Tait-Bryan angles.
	[[nodiscard]] Vector3<float> Rotated(Degrees yaw, Degrees pitch, Degrees roll) const {
		return Rotated(yaw.ToRad(), pitch.ToRad(), roll.ToRad());
	}

	/// @return True if all components are zero (or very close to zero within a small epsilon).
	[[nodiscard]] bool IsZero() const {
		return NearlyEqual(x, T{ 0 }) && NearlyEqual(y, T{ 0 }) && NearlyEqual(z, T{ 0 });
	}

	/// @return True if any component is zero (or very close to zero within a small epsilon).
	[[nodiscard]] bool HasZero() const {
		return NearlyEqual(x, T{ 0 }) || NearlyEqual(y, T{ 0 }) || NearlyEqual(z, T{ 0 });
	}

	/// @return True if all components are greater than zero. Returns false if any component is
	/// zero (or very close to zero within a small epsilon).
	[[nodiscard]] bool AllAboveZero() const {
		return x > 0 && y > 0 && z > 0 && !HasZero();
	}
};

template <Arithmetic T>
void to_json(json& j, const Vector3<T>& vector);

template <Arithmetic T>
void from_json(const json& j, Vector3<T>& vector);

using V3_int   = Vector3<int>;
using V3_uint  = Vector3<unsigned int>;
using V3_float = Vector3<float>;

template <Arithmetic V>
std::ostream& operator<<(std::ostream& os, Vector3<V> v) { // NOSONAR
	os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
	return os;
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
constexpr Vector3<S> operator+(Vector3<V> lhs, Vector3<U> rhs) { // NOSONAR
	return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
constexpr Vector3<S> operator-(Vector3<V> lhs, Vector3<U> rhs) { // NOSONAR
	return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
constexpr Vector3<S> operator*(Vector3<V> lhs, Vector3<U> rhs) { // NOSONAR
	return { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
constexpr Vector3<S> operator/(Vector3<V> lhs, Vector3<U> rhs) { // NOSONAR
	return { lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
constexpr Vector3<S> operator*(V lhs, Vector3<U> rhs) { // NOSONAR
	return { lhs * rhs.x, lhs * rhs.y, lhs * rhs.z };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
constexpr Vector3<S> operator*(Vector3<V> lhs, U rhs) { // NOSONAR
	return { lhs.x * rhs, lhs.y * rhs, lhs.z * rhs };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
constexpr Vector3<S> operator/(V lhs, Vector3<U> rhs) { // NOSONAR
	return { lhs / rhs.x, lhs / rhs.y, lhs / rhs.z };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
constexpr Vector3<S> operator/(Vector3<V> lhs, U rhs) { // NOSONAR
	return { lhs.x / rhs, lhs.y / rhs, lhs.z / rhs };
}

} // namespace ptgn

template <ptgn::Arithmetic T>
struct std::hash<ptgn::Vector3<T>> {
	std::size_t operator()(ptgn::Vector3<T> v) const noexcept {
		return ptgn::Hash(v.x, v.y, v.z);
	}
};