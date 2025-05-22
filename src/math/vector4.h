#pragma once

#include <iosfwd>
#include <ostream>
#include <type_traits>

#include "common/assert.h"
#include "common/type_traits.h"
#include "serialization/serializable.h"

namespace ptgn {

template <typename T, tt::arithmetic<T> = true>
struct Vector4 {
	T x{ 0 };
	T y{ 0 };
	T z{ 0 };
	T w{ 0 };

	constexpr Vector4()					   = default;
	~Vector4()							   = default;
	Vector4(const Vector4&)				   = default;
	Vector4(Vector4&&) noexcept			   = default;
	Vector4& operator=(const Vector4&)	   = default;
	Vector4& operator=(Vector4&&) noexcept = default;

	explicit constexpr Vector4(T all) : x{ all }, y{ all }, z{ all }, w{ all } {}

	constexpr Vector4(T x_component, T y_component, T z_component, T w_component) :
		x{ x_component }, y{ y_component }, z{ z_component }, w{ w_component } {}

	// TODO: Check that not_narrowing actually works as intended and static cast is not narrowing.
	template <typename U, tt::not_narrowing<U, T> = true>
	constexpr Vector4(const Vector4<U>& o) :
		x{ static_cast<T>(o.x) },
		y{ static_cast<T>(o.y) },
		z{ static_cast<T>(o.z) },
		w{ static_cast<T>(o.w) } {}

	// Note: use of explicit keyword for narrowing constructors.

	template <typename U, tt::narrowing<U, T> = true>
	explicit constexpr Vector4(U x_component, U y_component, U z_component, U w_component) :
		x{ static_cast<T>(x_component) },
		y{ static_cast<T>(y_component) },
		z{ static_cast<T>(z_component) },
		w{ static_cast<T>(w_component) } {}

	template <typename U, tt::narrowing<U, T> = true>
	explicit constexpr Vector4(const Vector4<U>& o) :
		x{ static_cast<T>(o.x) },
		y{ static_cast<T>(o.y) },
		z{ static_cast<T>(o.z) },
		w{ static_cast<T>(o.w) } {}

	// Access vector elements by index, 0 for x, 1 for y, 2 for z, 3 for w.
	[[nodiscard]] constexpr T& operator[](std::size_t idx) {
		if (idx == 1) {
			return y;
		} else if (idx == 2) {
			return z;
		} else if (idx == 3) {
			return w;
		}
		return x; // 0
	}

	// Access vector elements by index, 0 for x, 1 for y, 2 for z, 3 for w.
	[[nodiscard]] constexpr T operator[](std::size_t idx) const {
		if (idx == 1) {
			return y;
		} else if (idx == 2) {
			return z;
		} else if (idx == 3) {
			return w;
		}
		return x; // 0
	}

	[[nodiscard]] constexpr Vector4 operator-() const {
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

	PTGN_SERIALIZER_REGISTER(Vector4<T>, x, y, z, w)
};

using V4_int	= Vector4<int>;
using V4_uint	= Vector4<unsigned int>;
using V4_float	= Vector4<float>;
using V4_double = Vector4<double>;

template <typename V>
[[nodiscard]] inline bool operator==(const Vector4<V>& lhs, const Vector4<V>& rhs) {
	return NearlyEqual(lhs.x, rhs.x) && NearlyEqual(lhs.y, rhs.y) && NearlyEqual(lhs.z, rhs.z) &&
		   NearlyEqual(lhs.w, rhs.w);
}

template <typename V>
[[nodiscard]] inline bool operator!=(const Vector4<V>& lhs, const Vector4<V>& rhs) {
	return !operator==(lhs, rhs);
}

template <typename V, ptgn::tt::stream_writable<std::ostream, V> = true>
inline std::ostream& operator<<(std::ostream& os, const ptgn::Vector4<V>& v) {
	os << "(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")";
	return os;
}

template <typename V, typename U, typename S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector4<S> operator+(const Vector4<V>& lhs, const Vector4<U>& rhs) {
	return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w };
}

template <typename V, typename U, typename S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector4<S> operator-(const Vector4<V>& lhs, const Vector4<U>& rhs) {
	return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w };
}

template <typename V, typename U, typename S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector4<S> operator*(const Vector4<V>& lhs, const Vector4<U>& rhs) {
	return { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w };
}

template <typename V, typename U, typename S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector4<S> operator/(const Vector4<V>& lhs, const Vector4<U>& rhs) {
	return { lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z, lhs.w / rhs.w };
}

template <
	typename V, typename U, tt::arithmetic<V> = true,
	typename S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector4<S> operator*(V lhs, const Vector4<U>& rhs) {
	return { lhs * rhs.x, lhs * rhs.y, lhs * rhs.z, lhs * rhs.w };
}

template <
	typename V, typename U, tt::arithmetic<U> = true,
	typename S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector4<S> operator*(const Vector4<V>& lhs, U rhs) {
	return { lhs.x * rhs, lhs.y * rhs, lhs.z * rhs, lhs.w * rhs };
}

template <
	typename V, typename U, tt::arithmetic<V> = true,
	typename S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector4<S> operator/(V lhs, const Vector4<U>& rhs) {
	return { lhs / rhs.x, lhs / rhs.y, lhs / rhs.z, lhs / rhs.w };
}

template <
	typename V, typename U, tt::arithmetic<U> = true,
	typename S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector4<S> operator/(const Vector4<V>& lhs, U rhs) {
	return { lhs.x / rhs, lhs.y / rhs, lhs.z / rhs, lhs.w / rhs };
}

// Clamp all components of the vector between min and max (component specific).
template <typename T>
[[nodiscard]] inline Vector4<T> Clamp(
	const Vector4<T>& vector, const Vector4<T>& min, const Vector4<T>& max
) {
	return { std::clamp(vector.x, min.x, max.x), std::clamp(vector.y, min.y, max.y),
			 std::clamp(vector.z, min.z, max.z), std::clamp(vector.w, min.w, max.w) };
}

} // namespace ptgn

// Custom hashing function for Vector4 class.
// This allows for use of unordered maps and sets with Vector2s as keys.
template <typename T>
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