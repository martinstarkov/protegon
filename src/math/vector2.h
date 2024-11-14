#pragma once

#include <cmath>
#include <iosfwd>
#include <ostream>
#include <type_traits>

#include "math/math.h"
#include "math/rng.h"
#include "utility/debug.h"
#include "utility/type_traits.h"

// TODO: Add xyz() and xyzw() functions.
// TODO: Scrap support for int and stick to float/double. Do the same in all vectors and matrix4.

namespace ptgn {

struct Color;
struct LayerInfo;

namespace impl {

void DrawPoint(float x, float y, const Color& color, float radius, const LayerInfo& layer_info);

} // namespace impl

template <typename T>
struct Vector2 {
	static_assert(std::is_arithmetic_v<T>);

	T x{ 0 };
	T y{ 0 };

	constexpr Vector2()					   = default;
	~Vector2()							   = default;
	Vector2(const Vector2&)				   = default;
	Vector2(Vector2&&) noexcept			   = default;
	Vector2& operator=(const Vector2&)	   = default;
	Vector2& operator=(Vector2&&) noexcept = default;

	explicit constexpr Vector2(T all) : x{ all }, y{ all } {}

	constexpr Vector2(T x, T y) : x{ x }, y{ y } {}

	// TODO: Check that not_narrowing actually works as intended and static cast is not narrowing.
	template <typename U, tt::not_narrowing<U, T> = true>
	constexpr Vector2(const Vector2<U>& o) : x{ static_cast<T>(o.x) }, y{ static_cast<T>(o.y) } {}

	// Note: use of explicit keyword for narrowing constructors.

	template <typename U, tt::narrowing<U, T> = true>
	explicit constexpr Vector2(U x, U y) : x{ static_cast<T>(x) }, y{ static_cast<T>(y) } {}

	template <typename U, tt::narrowing<U, T> = true>
	explicit constexpr Vector2(const Vector2<U>& o) :
		x{ static_cast<T>(o.x) }, y{ static_cast<T>(o.y) } {}

	void Draw(const Color& color, float radius = 1.0f, const LayerInfo& layer_info = {}) const {
		impl::DrawPoint(static_cast<float>(x), static_cast<float>(y), color, radius, layer_info);
	}

	// Access vector elements by index, 0 for x, 1 for y.
	[[nodiscard]] constexpr T& operator[](std::size_t idx) {
		PTGN_ASSERT(idx >= 0 && idx < 2, "Vector2 subscript out of range");
		if (idx == 1) {
			return y;
		}
		return x; // idx == 0
	}

	// Access vector elements by index, 0 for x, 1 for y.
	[[nodiscard]] constexpr T operator[](std::size_t idx) const {
		PTGN_ASSERT(idx >= 0 && idx < 2, "Vector2 subscript out of range");
		if (idx == 1) {
			return y;
		}
		return x; // idx == 0
	}

	[[nodiscard]] constexpr Vector2 operator-() const {
		return { -x, -y };
	}

	template <typename U, tt::not_narrowing<U, T> = true>
	constexpr Vector2& operator+=(const Vector2<U>& rhs) {
		x += rhs.x;
		y += rhs.y;
		return *this;
	}

	template <typename U, tt::not_narrowing<U, T> = true>
	constexpr Vector2& operator-=(const Vector2<U>& rhs) {
		x -= rhs.x;
		y -= rhs.y;
		return *this;
	}

	template <typename U, tt::not_narrowing<U, T> = true>
	constexpr Vector2& operator*=(const Vector2<U>& rhs) {
		x *= rhs.x;
		y *= rhs.y;
		return *this;
	}

	template <typename U, tt::not_narrowing<U, T> = true>
	constexpr Vector2& operator/=(const Vector2<U>& rhs) {
		x /= rhs.x;
		y /= rhs.y;
		return *this;
	}

	template <typename U, tt::not_narrowing<U, T> = true>
	constexpr Vector2& operator*=(U rhs) {
		x *= rhs;
		y *= rhs;
		return *this;
	}

	template <typename U, tt::not_narrowing<U, T> = true>
	constexpr Vector2& operator/=(U rhs) {
		x /= rhs;
		y /= rhs;
		return *this;
	}

	// Returns the dot product (this * o).
	[[nodiscard]] constexpr T Dot(const Vector2& o) const {
		return x * o.x + y * o.y;
	}

	// Returns the cross product (this x o).
	[[nodiscard]] constexpr T Cross(const Vector2& o) const {
		return x * o.y - y * o.x;
	}

	[[nodiscard]] constexpr Vector2 Skewed() const {
		return { -y, x };
	}

	[[nodiscard]] constexpr Vector2 Swapped() const {
		return { y, x };
	}

	template <typename S = typename std::common_type_t<T, float>>
	[[nodiscard]] constexpr S Magnitude() const {
		static_assert(std::is_floating_point_v<S>, "Function requires floating point type");
		return std::sqrt(static_cast<S>(MagnitudeSquared()));
	}

	[[nodiscard]] constexpr T MagnitudeSquared() const {
		return Dot(*this);
	}

	[[nodiscard]] static Vector2 Random(T min, T max) {
		RNG<T> rng{ min, max };
		return { rng(), rng() };
	}

	[[nodiscard]] static Vector2 Random(const Vector2& min, const Vector2& max) {
		RNG<T> rng_x{ min.x, max.x };
		RNG<T> rng_y{ min.y, max.y };
		return { rng_x(), rng_y() };
	}

	[[nodiscard]] static Vector2 Right() {
		return { T{ 1 }, T{ 0 } };
	}

	[[nodiscard]] static Vector2 Left() {
		return { T{ -1 }, T{ 0 } };
	}

	[[nodiscard]] static Vector2 Up() {
		return { T{ 0 }, T{ 1 } };
	}

	[[nodiscard]] static Vector2 Down() {
		return { T{ 0 }, T{ -1 } };
	}

	// @return Random unit vector in a heading within the given range of angles (radians).
	[[nodiscard]] static Vector2 RandomHeading(
		T min_angle_radians = T{ 0 }, T max_angle_radians = T{ two_pi<T> }
	) {
		static_assert(std::is_floating_point_v<T>, "Function requires floating point type");
		RNG<T> heading_rng{ ClampAngle2Pi(min_angle_radians), ClampAngle2Pi(max_angle_radians) };
		T heading{ heading_rng() };
		return { std::cos(heading), std::sin(heading) };
	}

	// Returns a unit vector (magnitude = 1) except for zero vectors (magnitude
	// = 0).
	template <typename S = typename std::common_type_t<T, float>>
	[[nodiscard]] Vector2<S> Normalized() const {
		static_assert(std::is_floating_point_v<S>, "Function requires floating point type");
		T m{ MagnitudeSquared() };
		if (NearlyEqual(m, T{ 0 })) {
			return *this;
		}
		return *this / std::sqrt(static_cast<S>(m));
	}

	// Returns a normalized (unit) direction vector toward a target position.
	template <typename S = typename std::common_type_t<T, float>>
	[[nodiscard]] Vector2<S> DirectionTowards(const Vector2& target) const {
		static_assert(std::is_floating_point_v<S>, "Function requires floating point type");
		Vector2<S> dir{ target - *this };
		return dir.Normalized();
	}

	// @return New vector rotated counter-clockwise by the given angle.
	// See https://en.wikipedia.org/wiki/Rotation_matrix for details.
	// Angle in radians.
	template <typename S = typename std::common_type_t<T, float>>
	[[nodiscard]] Vector2<S> Rotated(S angle_radians) const {
		static_assert(std::is_floating_point_v<S>, "Function requires floating point type");
		auto c{ std::cos(angle_radians) };
		auto s{ std::sin(angle_radians) };
		return { x * c - y * s, x * s + y * c };
	}

	/*
	 * @return Angle in radians between vector x and y components in radians.
	 * Relative to the horizontal x-axis.
	 * Range: [-3.14159, 3.14159).
	 * (counter-clockwise positive).
	 *             1.5708
	 *               |
	 *    3.14159 ---o--- 0
	 *               |
	 *            -1.5708
	 */
	template <typename S = typename std::common_type_t<T, float>>
	[[nodiscard]] S Angle() const {
		static_assert(std::is_floating_point_v<S>, "Function requires floating point type");
		return std::atan2(static_cast<S>(y), static_cast<S>(x));
	}

	[[nodiscard]] bool IsZero() const {
		return NearlyEqual(x, T{ 0 }) && NearlyEqual(y, T{ 0 });
	}
};

using V2_int	= Vector2<int>;
using V2_uint	= Vector2<unsigned int>;
using V2_float	= Vector2<float>;
using V2_double = Vector2<double>;

template <typename S>
[[nodiscard]] inline bool operator==(const Vector2<S>& lhs, const Vector2<S>& rhs) {
	return NearlyEqual(lhs.x, rhs.x) && NearlyEqual(lhs.y, rhs.y);
}

template <typename S>
[[nodiscard]] inline bool operator!=(const Vector2<S>& lhs, const Vector2<S>& rhs) {
	return !operator==(lhs, rhs);
}

template <typename S, ptgn::tt::stream_writable<std::ostream, S> = true>
inline std::ostream& operator<<(std::ostream& os, const ptgn::Vector2<S>& v) {
	os << "(" << v.x << ", " << v.y << ")";
	return os;
}

template <typename V, typename U, typename S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector2<S> operator+(const Vector2<V>& lhs, const Vector2<U>& rhs) {
	return { lhs.x + rhs.x, lhs.y + rhs.y };
}

template <typename V, typename U, typename S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector2<S> operator-(const Vector2<V>& lhs, const Vector2<U>& rhs) {
	return { lhs.x - rhs.x, lhs.y - rhs.y };
}

template <typename V, typename U, typename S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector2<S> operator*(const Vector2<V>& lhs, const Vector2<U>& rhs) {
	return { lhs.x * rhs.x, lhs.y * rhs.y };
}

template <typename V, typename U, typename S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector2<S> operator/(const Vector2<V>& lhs, const Vector2<U>& rhs) {
	return { lhs.x / rhs.x, lhs.y / rhs.y };
}

template <
	typename V, typename U, tt::arithmetic<V> = true,
	typename S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector2<S> operator*(V lhs, const Vector2<U>& rhs) {
	return { lhs * rhs.x, lhs * rhs.y };
}

template <
	typename V, typename U, tt::arithmetic<U> = true,
	typename S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector2<S> operator*(const Vector2<V>& lhs, U rhs) {
	return { lhs.x * rhs, lhs.y * rhs };
}

template <
	typename V, typename U, tt::arithmetic<V> = true,
	typename S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector2<S> operator/(V lhs, const Vector2<U>& rhs) {
	return { lhs / rhs.x, lhs / rhs.y };
}

template <
	typename V, typename U, tt::arithmetic<U> = true,
	typename S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector2<S> operator/(const Vector2<V>& lhs, U rhs) {
	return { lhs.x / rhs, lhs.y / rhs };
}

template <typename T>
[[nodiscard]] inline Vector2<T> Clamp(
	const Vector2<T>& vector, const Vector2<T>& min, const Vector2<T>& max
) {
	return { std::clamp(vector.x, min.x, max.x), std::clamp(vector.y, min.y, max.y) };
}

template <typename T>
[[nodiscard]] inline Vector2<T> Clamp(const Vector2<T>& vector, T min, T max) {
	Vector2<T> dir{ vector.Normalized() };
	Vector2<T> dir_min{ dir * Vector2<T>{ min, min } };
	Vector2<T> dir_max{ dir * Vector2<T>{ max, max } };

	Vector2<T> min_v{ std::min(dir_min.x, dir_max.x), std::min(dir_min.y, dir_max.y) };
	Vector2<T> max_v{ std::max(dir_min.x, dir_max.x), std::max(dir_min.y, dir_max.y) };

	return Clamp(vector, min_v, max_v);
}

template <typename T>
[[nodiscard]] inline Vector2<T> Round(const Vector2<T>& vector) {
	return { std::round(vector.x), std::round(vector.y) };
}

template <typename T>
[[nodiscard]] inline Vector2<T> Lerp(const Vector2<T>& lhs, const Vector2<T>& rhs, T t) {
	return Vector2<T>{ Lerp(lhs.x, rhs.x, t), Lerp(lhs.y, rhs.y, t) };
}

template <typename T>
[[nodiscard]] inline Vector2<T> Midpoint(const Vector2<T>& lhs, const Vector2<T>& rhs) {
	return Vector2<T>{ (lhs + rhs) / 2.0f };
}

} // namespace ptgn

// Custom hashing function for Vector2 class.
// This allows for use of unordered maps and sets with Vector2s as keys.
template <typename T>
struct std::hash<ptgn::Vector2<T>> {
	std::size_t operator()(const ptgn::Vector2<T>& v) const noexcept {
		// Hashing combination algorithm from:
		// https://stackoverflow.com/a/17017281
		std::size_t hash{ 17 };
		hash = hash * 31 + std::hash<T>()(v.x);
		hash = hash * 31 + std::hash<T>()(v.y);
		return hash;
	}
};