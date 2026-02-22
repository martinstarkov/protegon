#pragma once

#include <array>
#include <cmath>
#include <functional>
#include <iosfwd>
#include <limits>
#include <ostream>
#include <type_traits>

#include "common/assert.h"
#include "common/concepts.h"
#include "math/math.h"
#include "math/rng.h"
#include "math/tolerance.h"
#include "serialization/fwd.h"

// TODO: Add xyz() and xyzw() functions.

namespace ptgn {

template <Arithmetic T>
struct Vector2 {
	T x{ 0 };
	T y{ 0 };

	constexpr Vector2() = default;

	template <Arithmetic U>
	explicit constexpr Vector2(U all) : x{ static_cast<T>(all) }, y{ static_cast<T>(all) } {}

	explicit Vector2(const json& j);

	template <Arithmetic U>
	constexpr Vector2(const Vector2<U>& o) : x{ static_cast<T>(o.x) }, y{ static_cast<T>(o.y) } {}

	template <ConvertibleToArithmetic U, ConvertibleToArithmetic S>
	constexpr Vector2(U x_component, S y_component) :
		x{ static_cast<T>(x_component) }, y{ static_cast<T>(y_component) } {}

	template <Arithmetic U>
	constexpr Vector2(const std::array<U, 2>& o) :
		x{ static_cast<T>(o[0]) }, y{ static_cast<T>(o[1]) } {}

	friend bool operator==(const Vector2& lhs, const Vector2& rhs) {
		return NearlyEqual(lhs.x, rhs.x) && NearlyEqual(lhs.y, rhs.y);
	}

	// Access vector elements by index, 0 for x, 1 for y.
	[[nodiscard]] constexpr T& operator[](std::size_t idx) {
		if (idx == 1) {
			return y;
		}
		return x; // idx == 0
	}

	// Access vector elements by index, 0 for x, 1 for y.
	[[nodiscard]] constexpr T operator[](std::size_t idx) const {
		if (idx == 1) {
			return y;
		}
		return x; // idx == 0
	}

	[[nodiscard]] constexpr Vector2 operator-() const {
		return { -x, -y };
	}

	template <Arithmetic U>
		requires NotNarrowingArithmetic<U, T>
	constexpr Vector2& operator+=(const Vector2<U>& rhs) {
		x += rhs.x;
		y += rhs.y;
		return *this;
	}

	template <Arithmetic U>
		requires NotNarrowingArithmetic<U, T>
	constexpr Vector2& operator-=(const Vector2<U>& rhs) {
		x -= rhs.x;
		y -= rhs.y;
		return *this;
	}

	template <Arithmetic U>
		requires NotNarrowingArithmetic<U, T>
	constexpr Vector2& operator*=(const Vector2<U>& rhs) {
		x *= rhs.x;
		y *= rhs.y;
		return *this;
	}

	template <Arithmetic U>
		requires NotNarrowingArithmetic<U, T>
	constexpr Vector2& operator/=(const Vector2<U>& rhs) {
		x /= rhs.x;
		y /= rhs.y;
		return *this;
	}

	template <Arithmetic U>
		requires NotNarrowingArithmetic<U, T>
	constexpr Vector2& operator*=(U rhs) {
		x *= rhs;
		y *= rhs;
		return *this;
	}

	template <Arithmetic U>
		requires NotNarrowingArithmetic<U, T>
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

	[[nodiscard]] constexpr T MagnitudeSquared() const {
		return Dot(*this);
	}

	[[nodiscard]] static Vector2 Random(T min, T max) {
		RNG<T> rng{ min, max };
		return { rng(), rng() };
	}

	[[nodiscard]] static Vector2 RandomNormalized(T min, T max) {
		auto dir{ Vector2::Random(min, max) };
		if (dir.IsZero()) {
			return Vector2{};
		} else {
			return Vector2{ dir.Normalized() };
		}
	}

	[[nodiscard]] static Vector2 Random(const Vector2& min, const Vector2& max) {
		RNG<T> rng_x{ min.x, max.x };
		RNG<T> rng_y{ min.y, max.y };
		return { rng_x(), rng_y() };
	}

	[[nodiscard]] static Vector2 Right() {
		return { T{ 1 }, T{ 0 } };
	}

	[[nodiscard]] static Vector2 Up() {
		return { T{ 0 }, T{ 1 } };
	}

	[[nodiscard]] static Vector2 Left() {
		return { T{ -1 }, T{ 0 } };
	}

	[[nodiscard]] static Vector2 Down() {
		return { T{ 0 }, T{ -1 } };
	}

	[[nodiscard]] static Vector2 Infinity() {
		return { std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() };
	}

	template <std::floating_point S = typename std::common_type_t<T, float>>
	[[nodiscard]] constexpr S Magnitude() const {
		return std::sqrt(static_cast<S>(MagnitudeSquared()));
	}

	// @return Random unit vector in a heading within the given range of angles (radians).
	template <std::floating_point S = typename std::common_type_t<T, float>>
	[[nodiscard]] static Vector2 RandomHeading(
		S min_angle_radians = S{ 0 }, S max_angle_radians = S{ two_pi<S> }
	) {
		RNG<S> heading_rng{ ClampAngle2Pi(min_angle_radians), ClampAngle2Pi(max_angle_radians) };
		S heading{ heading_rng() };
		return { std::cos(heading), std::sin(heading) };
	}

	// Returns a unit vector (magnitude = 1) except for zero vectors (magnitude
	// = 0).
	template <std::floating_point S = typename std::common_type_t<T, float>>
	[[nodiscard]] Vector2<S> Normalized() const {
		T m{ MagnitudeSquared() };
		if (NearlyEqual(m, T{ 0 })) {
			return *this;
		}
		return *this / std::sqrt(static_cast<S>(m));
	}

	// Returns a normalized (unit) direction vector toward a target position.
	template <std::floating_point S = typename std::common_type_t<T, float>>
	[[nodiscard]] Vector2<S> DirectionTowards(const Vector2& target) const {
		Vector2<S> dir{ target - *this };
		return dir.Normalized();
	}

	// @return New vector rotated by the given angle.
	// See https://en.wikipedia.org/wiki/Rotation_matrix for details.
	// Angle in radians. Positive angle rotates clockwise.
	template <std::floating_point S = typename std::common_type_t<T, float>>
	[[nodiscard]] Vector2<S> Rotated(S angle_radians) const {
		if (NearlyEqual(angle_radians, 0.0f)) {
			return { x, y };
		}
		auto c{ std::cos(angle_radians) };
		auto s{ std::sin(angle_radians) };
		return Rotated(c, s);
	}

	// Provide cached std::cos(angle_radians) and std::sin(angle_radians) values.
	template <std::floating_point S = typename std::common_type_t<T, float>>
	[[nodiscard]] Vector2<S> Rotated(S cos_angle_radians, S sin_angle_radians) const {
		return { x * cos_angle_radians - y * sin_angle_radians,
				 x * sin_angle_radians + y * cos_angle_radians };
	}

	/*
	 * @return Angle in radians between vector x and y components in radians.
	 * Relative to the horizontal x-axis (1, 0).
	 * Range: (-3.14159, 3.14159].
	 * (clockwise positive).
	 *           -1.5708
	 *               |
	 *    3.14159 ---o--- 0
	 *               |
	 *            1.5708
	 */
	template <std::floating_point S = typename std::common_type_t<T, float>>
	[[nodiscard]] S Angle() const {
		return std::atan2(static_cast<S>(y), static_cast<S>(x));
	}

	template <std::floating_point S = typename std::common_type_t<T, float>>
	[[nodiscard]] S Angle(const Vector2& target) const {
		S mag1{ MagnitudeSquared() };
		S mag2{ target.MagnitudeSquared() };

		if (NearlyEqual(mag1, S{ 0 }) || NearlyEqual(mag2, S{ 0 })) {
			return S{ 0 };
		}

		S cosine{ Dot(target) / std::sqrt(mag1 * mag2) };

		// Clamp cosine to the range [-1, 1] to avoid domain errors for acos. This can very rarely
		// happen due to floating point inaccuracies.
		cosine = std::clamp(cosine, S{ -1 }, S{ 1 });

		return std::acos(cosine);
	}

	// @return True if both components are zero.
	[[nodiscard]] bool IsZero() const {
		return NearlyEqual(x, T{ 0 }) && NearlyEqual(y, T{ 0 });
	}

	// @return True if either component is zero.
	[[nodiscard]] bool HasZero() const {
		return NearlyEqual(x, T{ 0 }) || NearlyEqual(y, T{ 0 });
	}

	[[nodiscard]] bool BothAboveZero() const {
		return x > 0 && y > 0 && !HasZero();
	}
};

template <Arithmetic T>
void to_json(json& j, const Vector2<T>& vector);

template <Arithmetic T>
void from_json(const json& j, Vector2<T>& vector);

using V2_int	= Vector2<int>;
using V2_uint	= Vector2<unsigned int>;
using V2_size	= Vector2<std::size_t>;
using V2_float	= Vector2<float>;
using V2_double = Vector2<double>;

template <StreamWritable S> /* Some types such as std::uint8_t are not stream writable */
inline std::ostream& operator<<(std::ostream& os, const ptgn::Vector2<S>& v) {
	os << "(" << v.x << ", " << v.y << ")";
	return os;
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector2<S> operator+(const Vector2<V>& lhs, const Vector2<U>& rhs) {
	return { static_cast<S>(lhs.x) + static_cast<S>(rhs.x),
			 static_cast<S>(lhs.y) + static_cast<S>(rhs.y) };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector2<S> operator-(const Vector2<V>& lhs, const Vector2<U>& rhs) {
	return { static_cast<S>(lhs.x) - static_cast<S>(rhs.x),
			 static_cast<S>(lhs.y) - static_cast<S>(rhs.y) };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector2<S> operator*(const Vector2<V>& lhs, const Vector2<U>& rhs) {
	return { static_cast<S>(lhs.x) * static_cast<S>(rhs.x),
			 static_cast<S>(lhs.y) * static_cast<S>(rhs.y) };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector2<S> operator/(const Vector2<V>& lhs, const Vector2<U>& rhs) {
	return { static_cast<S>(lhs.x) / static_cast<S>(rhs.x),
			 static_cast<S>(lhs.y) / static_cast<S>(rhs.y) };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>

[[nodiscard]] constexpr Vector2<S> operator*(V lhs, const Vector2<U>& rhs) {
	return { static_cast<S>(lhs) * static_cast<S>(rhs.x),
			 static_cast<S>(lhs) * static_cast<S>(rhs.y) };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
[[nodiscard]] constexpr Vector2<S> operator*(const Vector2<V>& lhs, U rhs) {
	return { static_cast<S>(lhs.x) * static_cast<S>(rhs),
			 static_cast<S>(lhs.y) * static_cast<S>(rhs) };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>

[[nodiscard]] constexpr Vector2<S> operator/(V lhs, const Vector2<U>& rhs) {
	return { static_cast<S>(lhs) / static_cast<S>(rhs.x),
			 static_cast<S>(lhs) / static_cast<S>(rhs.y) };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>

[[nodiscard]] constexpr Vector2<S> operator/(const Vector2<V>& lhs, U rhs) {
	return { static_cast<S>(lhs.x) / static_cast<S>(rhs),
			 static_cast<S>(lhs.y) / static_cast<S>(rhs) };
}

// Clamp both components of a vector between min and max (component specific).
template <Arithmetic T>
[[nodiscard]] inline Vector2<T> Clamp(
	const Vector2<T>& vector, const Vector2<T>& min, const Vector2<T>& max
) {
	return { std::clamp(vector.x, min.x, max.x), std::clamp(vector.y, min.y, max.y) };
}

// Clamp the magnitude of the vector between min and max. This means that a (1, 1) vector clamped
// between -1 and 1 will be (0.7, 0.7)
template <Arithmetic T>
[[nodiscard]] inline Vector2<T> Clamp(const Vector2<T>& vector, T min, T max) {
	Vector2<T> dir{ vector.Normalized() };
	Vector2<T> dir_min{ dir * Vector2<T>{ min, min } };
	Vector2<T> dir_max{ dir * Vector2<T>{ max, max } };

	Vector2<T> min_v{ std::min(dir_min.x, dir_max.x), std::min(dir_min.y, dir_max.y) };
	Vector2<T> max_v{ std::max(dir_min.x, dir_max.x), std::max(dir_min.y, dir_max.y) };

	return Clamp(vector, min_v, max_v);
}

// @return True if both the components of a and b are within margin of each other.
template <Arithmetic T>
[[nodiscard]] inline bool WithinMargin(
	const Vector2<T>& a, const Vector2<T>& b, const Vector2<T>& margin
) {
	return std::abs(a.x - b.x) <= margin.x && std::abs(a.y - b.y) <= margin.y;
}

// Ceil both components of a vector.
template <Arithmetic T>
[[nodiscard]] inline Vector2<T> Ceil(const Vector2<T>& vector) {
	return { Ceil(vector.x), Ceil(vector.y) };
}

// Floor both components of a vector.
template <Arithmetic T>
[[nodiscard]] inline Vector2<T> Floor(const Vector2<T>& vector) {
	return { Floor(vector.x), Floor(vector.y) };
}

// Round both components of a vector.
template <Arithmetic T>
[[nodiscard]] inline Vector2<T> Round(const Vector2<T>& vector) {
	return { Round(vector.x), Round(vector.y) };
}

// Absolute value for both components of a vector.
template <Arithmetic T>
[[nodiscard]] inline Vector2<T> Abs(const Vector2<T>& vector) {
	return { Abs(vector.x), Abs(vector.y) };
}

// Swap both components of vectors a and b.
template <Arithmetic T>
inline void Swap(Vector2<T>& a, Vector2<T>& b) {
	std::swap(a.x, b.x);
	std::swap(a.y, b.y);
}

// Linearly interpolate both components of a vector.
template <Arithmetic T>
[[nodiscard]] inline Vector2<T> Lerp(const Vector2<T>& lhs, const Vector2<T>& rhs, T t) {
	return Vector2<T>{ Lerp(lhs.x, rhs.x, t), Lerp(lhs.y, rhs.y, t) };
}

// Linearly interpolate both components of a vector by their respective t values.
template <Arithmetic T>
[[nodiscard]] inline Vector2<T> Lerp(
	const Vector2<T>& lhs, const Vector2<T>& rhs, const Vector2<T>& t
) {
	return Vector2<T>{ Lerp(lhs.x, rhs.x, t.x), Lerp(lhs.y, rhs.y, t.y) };
}

// @return The midpoint between vectors a and b.
template <Arithmetic T>
[[nodiscard]] inline Vector2<T> Midpoint(const Vector2<T>& a, const Vector2<T>& b) {
	return Vector2<T>{ (a + b) / 2.0f };
}

// @return The larger component of a vector.
template <Arithmetic T>
[[nodiscard]] inline T Max(const Vector2<T>& vector) {
	return std::max(vector.x, vector.y);
}

// @return The smaller component of a vector.
template <Arithmetic T>
[[nodiscard]] inline T Min(const Vector2<T>& vector) {
	return std::min(vector.x, vector.y);
}

} // namespace ptgn

// Custom hashing function for Vector2 class.
// This allows for use of unordered maps and sets with Vector2s as keys.
template <ptgn::Arithmetic T>
struct std::hash<ptgn::Vector2<T>> {
	std::size_t operator()(const ptgn::Vector2<T>& v) const noexcept {
		// Hashing combination algorithm from:
		// https://stackoverflow.com/a/17017281
		std::size_t value{ 17 };
		value = value * 31 + std::hash<T>()(v.x);
		value = value * 31 + std::hash<T>()(v.y);
		return value;
	}
};
