#pragma once

#include <array>
#include <cmath>
#include <functional>
#include <ostream>
#include <type_traits>

#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/util/concepts.h"
#include "serialization/json/fwd.h"

namespace ptgn {

template <Arithmetic T>
struct Vector4 {
	T x{ 0 };
	T y{ 0 };
	T z{ 0 };
	T w{ 0 };

	constexpr T* Data() noexcept {
		static_assert(std::is_standard_layout_v<Vector4>);
		return &x;
	}

	constexpr const T* Data() const noexcept {
		static_assert(std::is_standard_layout_v<Vector4>);
		return &x;
	}

	constexpr Vector4() = default;

	template <Arithmetic U>
	explicit constexpr Vector4(U all) :
		x{ static_cast<T>(all) },
		y{ static_cast<T>(all) },
		z{ static_cast<T>(all) },
		w{ static_cast<T>(all) } {}

	constexpr Vector4(T x_component, T y_component, T z_component, T w_component) :
		x{ x_component }, y{ y_component }, z{ z_component }, w{ w_component } {}

	explicit Vector4(const json& j);

	template <Arithmetic U>
	constexpr Vector4(Vector4<U> o) : // NOSONAR
		x{ static_cast<T>(o.x) },
		y{ static_cast<T>(o.y) },
		z{ static_cast<T>(o.z) },
		w{ static_cast<T>(o.w) } {}

	template <
		ConvertibleToArithmetic U, ConvertibleToArithmetic S, ConvertibleToArithmetic V,
		ConvertibleToArithmetic W>
	constexpr Vector4(U x_component, S y_component, V z_component, W w_component) :
		x{ static_cast<T>(x_component) },
		y{ static_cast<T>(y_component) },
		z{ static_cast<T>(z_component) },
		w{ static_cast<T>(w_component) } {}

	template <Arithmetic U>
	explicit constexpr Vector4(std::array<U, 4> o) :
		x{ static_cast<T>(o[0]) },
		y{ static_cast<T>(o[1]) },
		z{ static_cast<T>(o[2]) },
		w{ static_cast<T>(o[3]) } {}

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

	[[nodiscard]] constexpr Vector3<T> xyz() const {
		return { x, y, z };
	}

	friend bool operator==(const Vector4& lhs, const Vector4& rhs) {
		return NearlyEqual(lhs.x, rhs.x) && NearlyEqual(lhs.y, rhs.y) &&
			   NearlyEqual(lhs.z, rhs.z) && NearlyEqual(lhs.w, rhs.w);
	}

	/// @brief Access vector elements by index, 0 for x, 1 for y, 2 for z, 3 for w.
	constexpr T& operator[](std::size_t idx) {
		if (idx == 1) {
			return y;
		} else if (idx == 2) {
			return z;
		} else if (idx == 3) {
			return w;
		}
		return x; // 0
	}

	/// @brief Access vector elements by index, 0 for x, 1 for y, 2 for z, 3 for w.
	constexpr T operator[](std::size_t idx) const {
		if (idx == 1) {
			return y;
		} else if (idx == 2) {
			return z;
		} else if (idx == 3) {
			return w;
		}
		return x; // 0
	}

	constexpr Vector4 operator-() const {
		return { -x, -y, -z, -w };
	}

	template <Arithmetic U>
		requires NotNarrowingArithmetic<U, T>
	constexpr Vector4& operator+=(Vector4<U> rhs) {
		x += rhs.x;
		y += rhs.y;
		z += rhs.z;
		w += rhs.w;
		return *this;
	}

	template <Arithmetic U>
		requires NotNarrowingArithmetic<U, T>
	constexpr Vector4& operator-=(Vector4<U> rhs) {
		x -= rhs.x;
		y -= rhs.y;
		z -= rhs.z;
		w -= rhs.w;
		return *this;
	}

	template <Arithmetic U>
		requires NotNarrowingArithmetic<U, T>
	constexpr Vector4& operator*=(Vector4<U> rhs) {
		x *= rhs.x;
		y *= rhs.y;
		z *= rhs.z;
		w *= rhs.w;
		return *this;
	}

	template <Arithmetic U>
		requires NotNarrowingArithmetic<U, T>
	constexpr Vector4& operator/=(Vector4<U> rhs) {
		x /= rhs.x;
		y /= rhs.y;
		z /= rhs.z;
		w /= rhs.w;
		return *this;
	}

	template <Arithmetic U>
		requires NotNarrowingArithmetic<U, T>
	constexpr Vector4& operator*=(U rhs) {
		x *= rhs;
		y *= rhs;
		z *= rhs;
		w *= rhs;
		return *this;
	}

	template <Arithmetic U>
		requires NotNarrowingArithmetic<U, T>
	constexpr Vector4& operator/=(U rhs) {
		x /= rhs;
		y /= rhs;
		z /= rhs;
		w /= rhs;
		return *this;
	}

	/// @return The dot product (this * o).
	[[nodiscard]] constexpr T Dot(Vector4 o) const {
		return x * o.x + y * o.y + z * o.z + w * o.w;
	}

	[[nodiscard]] constexpr float Magnitude() const {
		return std::sqrt(static_cast<float>(MagnitudeSquared()));
	}

	[[nodiscard]] constexpr T MagnitudeSquared() const {
		return Dot(*this);
	}

	/// @return Unit vector (magnitude = 1) except for zero vectors (magnitude = 0).
	[[nodiscard]] Vector4<float> Normalized() const {
		T m{ MagnitudeSquared() };
		if (NearlyEqual(m, T{ 0 })) {
			return *this;
		}
		return *this / std::sqrt(static_cast<float>(m));
	}

	/// @return True if all components are in range [0.0, 1.0].
	[[nodiscard]] constexpr bool IsNormalized() const {
		return x >= 0.0f && x <= 1.0f && y >= 0.0f && y <= 1.0f && z >= 0.0f && z <= 1.0f &&
			   w >= 0.0f && w <= 1.0f;
	}

	/// @return True if all components are zero (or very close to zero within a small epsilon).
	[[nodiscard]] bool IsZero() const {
		return NearlyEqual(x, T{ 0 }) && NearlyEqual(y, T{ 0 }) && NearlyEqual(z, T{ 0 }) &&
			   NearlyEqual(w, T{ 0 });
	}

	/// @return True if any component is zero (or very close to zero within a small epsilon).
	[[nodiscard]] bool HasZero() const {
		return NearlyEqual(x, T{ 0 }) || NearlyEqual(y, T{ 0 }) || NearlyEqual(z, T{ 0 }) ||
			   NearlyEqual(w, T{ 0 });
	}

	/// @return True if all components are greater than zero. Returns false if any component is
	/// zero (or very close to zero within a small epsilon).
	[[nodiscard]] bool AllAboveZero() const {
		return x > 0 && y > 0 && z > 0 && w > 0 && !HasZero();
	}
};

template <Arithmetic T>
void to_json(json& j, const Vector4<T>& vector);

template <Arithmetic T>
void from_json(const json& j, Vector4<T>& vector);

using V4_int   = Vector4<int>;
using V4_uint  = Vector4<unsigned int>;
using V4_float = Vector4<float>;

template <Arithmetic V>
inline std::ostream& operator<<(std::ostream& os, Vector4<V> v) { // NOSONAR
	os << "(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")";
	return os;
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
constexpr Vector4<S> operator+(Vector4<V> lhs, Vector4<U> rhs) { // NOSONAR
	return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
constexpr Vector4<S> operator-(Vector4<V> lhs, Vector4<U> rhs) { // NOSONAR
	return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
constexpr Vector4<S> operator*(Vector4<V> lhs, Vector4<U> rhs) { // NOSONAR
	return { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
constexpr Vector4<S> operator/(Vector4<V> lhs, Vector4<U> rhs) { // NOSONAR
	return { lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z, lhs.w / rhs.w };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
constexpr Vector4<S> operator*(V lhs, Vector4<U> rhs) { // NOSONAR
	return { lhs * rhs.x, lhs * rhs.y, lhs * rhs.z, lhs * rhs.w };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
constexpr Vector4<S> operator*(Vector4<V> lhs, U rhs) { // NOSONAR
	return { lhs.x * rhs, lhs.y * rhs, lhs.z * rhs, lhs.w * rhs };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
constexpr Vector4<S> operator/(V lhs, Vector4<U> rhs) { // NOSONAR
	return { lhs / rhs.x, lhs / rhs.y, lhs / rhs.z, lhs / rhs.w };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
constexpr Vector4<S> operator/(Vector4<V> lhs, U rhs) { // NOSONAR
	return { lhs.x / rhs, lhs.y / rhs, lhs.z / rhs, lhs.w / rhs };
}

/// @brief Clamp all components of the vector between min and max (component specific).
template <Arithmetic T>
[[nodiscard]] inline Vector4<T> Clamp(Vector4<T> vector, Vector4<T> min, Vector4<T> max) {
	return { std::clamp(vector.x, min.x, max.x), std::clamp(vector.y, min.y, max.y),
			 std::clamp(vector.z, min.z, max.z), std::clamp(vector.w, min.w, max.w) };
}

} // namespace ptgn

/// Custom hashing function for Vector4 class.
/// This allows for use of unordered maps and sets with Vector2s as keys.
template <ptgn::Arithmetic T>
struct std::hash<ptgn::Vector4<T>> {
	std::size_t operator()(const ptgn::Vector4<T>& v) const noexcept {
		// Hashing combination algorithm from:
		// https://stackoverflow.com/a/17017281
		std::size_t value{ 17 };
		value = value * 31 + std::hash<T>()(v.x);
		value = value * 31 + std::hash<T>()(v.y);
		value = value * 31 + std::hash<T>()(v.z);
		value = value * 31 + std::hash<T>()(v.w);
		return value;
	}
};