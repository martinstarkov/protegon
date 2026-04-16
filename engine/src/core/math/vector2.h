#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <ostream>
#include <type_traits>

#include "core/math/angle.h"
#include "core/math/math_utils.h"
#include "core/math/rng.h"
#include "core/math/tolerance.h"
#include "core/util/concepts.h"
#include "serialization/json/fwd.h"

namespace ptgn {

template <Arithmetic T>
struct Vector2 {
	T x{ 0 };
	T y{ 0 };

	[[nodiscard]] constexpr T* Data() noexcept {
		static_assert(std::is_standard_layout_v<Vector2>);
		return &x;
	}

	[[nodiscard]] constexpr const T* Data() const noexcept {
		static_assert(std::is_standard_layout_v<Vector2>);
		return &x;
	}

	constexpr Vector2() = default;

	template <Arithmetic U>
	explicit constexpr Vector2(U both) : x{ static_cast<T>(both) }, y{ static_cast<T>(both) } {}

	template <Arithmetic U>
	constexpr Vector2(Vector2<U> o) // NOSONAR
		:
		x{ static_cast<T>(o.x) }, y{ static_cast<T>(o.y) } {}

	template <ConvertibleToArithmetic U, ConvertibleToArithmetic S>
	constexpr Vector2(U x_component, S y_component) :
		x{ static_cast<T>(x_component) }, y{ static_cast<T>(y_component) } {}

	template <Arithmetic U>
	explicit constexpr Vector2(std::array<U, 2> o) :
		x{ static_cast<T>(o[0]) }, y{ static_cast<T>(o[1]) } {}

	[[nodiscard]] constexpr Vector2 xx() {
		return { x, x };
	}

	[[nodiscard]] constexpr Vector2 yy() const {
		return { y, y };
	}

	friend bool operator==(Vector2 lhs, Vector2 rhs) {
		return NearlyEqual(lhs.x, rhs.x) && NearlyEqual(lhs.y, rhs.y);
	}

	/// @brief Access vector elements by index, 0 for x, 1 for y.
	constexpr T& operator[](std::size_t idx) {
		if (idx == 1) {
			return y;
		}
		return x; // idx == 0
	}

	/// @brief Access vector elements by index, 0 for x, 1 for y.
	constexpr T operator[](std::size_t idx) const {
		if (idx == 1) {
			return y;
		}
		return x; // idx == 0
	}

	constexpr Vector2 operator-() const {
		return { -x, -y };
	}

	template <Arithmetic U>
		requires NotNarrowingArithmetic<U, T>
	constexpr Vector2& operator+=(Vector2<U> rhs) {
		x += rhs.x;
		y += rhs.y;
		return *this;
	}

	template <Arithmetic U>
		requires NotNarrowingArithmetic<U, T>
	constexpr Vector2& operator-=(Vector2<U> rhs) {
		x -= rhs.x;
		y -= rhs.y;
		return *this;
	}

	template <Arithmetic U>
		requires NotNarrowingArithmetic<U, T>
	constexpr Vector2& operator*=(Vector2<U> rhs) {
		x *= rhs.x;
		y *= rhs.y;
		return *this;
	}

	template <Arithmetic U>
		requires NotNarrowingArithmetic<U, T>
	constexpr Vector2& operator/=(Vector2<U> rhs) {
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

	/// @return The dot product (this * o).
	[[nodiscard]] constexpr T Dot(Vector2 o) const {
		return x * o.x + y * o.y;
	}

	/// @return The cross product (this x o).
	[[nodiscard]] constexpr T Cross(Vector2 o) const {
		return x * o.y - y * o.x;
	}

	/// @return { -y, x }
	[[nodiscard]] constexpr Vector2 Skewed() const {
		return { -y, x };
	}

	/// @return { y, x }
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

	[[nodiscard]] static Vector2 Random(Vector2 min, Vector2 max) {
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

	[[nodiscard]] constexpr float Magnitude() const {
		return std::sqrt(static_cast<float>(MagnitudeSquared()));
	}

	/// @return Random unit vector in a heading within the given range of angles.
	[[nodiscard]] static Vector2 RandomHeading(
		Degrees min_angle = 0.0f, Degrees max_angle = 360.0f
	) {
		RNG<float> heading_rng{ Clamp(min_angle).value, Clamp(max_angle).value };
		Radians heading{ Degrees{ heading_rng() } };
		return { heading.Cos(), heading.Sin() };
	}

	/// @return Unit vector (magnitude = 1) except for zero vectors (magnitude = 0).
	[[nodiscard]] Vector2<float> Normalized() const {
		T m{ MagnitudeSquared() };
		if (NearlyEqual(m, T{ 0 })) {
			return *this;
		}
		return *this / std::sqrt(static_cast<float>(m));
	}

	/// @return Normalized (unit) direction vector toward a target position.
	[[nodiscard]] Vector2<float> DirectionTowards(Vector2 target) const {
		Vector2<float> dir{ target - *this };
		return dir.Normalized();
	}

	/// @brief See https://en.wikipedia.org/wiki/Rotation_matrix for details.
	/// Positive clockwise.
	/// @return New vector rotated by the given angle.
	[[nodiscard]] Vector2<float> Rotated(Radians angle) const {
		if (NearlyEqual(angle.value, 0.0f)) {
			return { x, y };
		}
		auto c{ angle.Cos() };
		auto s{ angle.Sin() };
		return Rotated(c, s);
	}

	/// @brief See https://en.wikipedia.org/wiki/Rotation_matrix for details.
	/// Positive clockwise.
	/// @return New vector rotated by the given angle.
	[[nodiscard]] Vector2<float> Rotated(Degrees angle) const {
		return Rotated(angle.ToRad());
	}

	/// @brief Provide cached std::cos(angle) and std::sin(angle) values.
	[[nodiscard]] Vector2<float> Rotated(float cos, float sin) const {
		return { x * cos - y * sin, x * sin + y * cos };
	}

	/// @return Angle in degrees between vector x and y components in radians.
	/// Relative to the horizontal x-axis (1, 0).
	/// Range: (-180.0f, 180.0f].
	/// Positive clockwise.
	///          -90
	///           |
	///    180 ---o--- 0
	///           |
	///           90
	[[nodiscard]] Degrees Angle() const {
		return Radians{ std::atan2(static_cast<float>(y), static_cast<float>(x)) }.ToDeg();
	}

	/// @brief Angle between this vector and a target vector in degrees.
	[[nodiscard]] Degrees Angle(Vector2 target) const {
		float mag1{ static_cast<float>(MagnitudeSquared()) };
		float mag2{ static_cast<float>(target.MagnitudeSquared()) };

		if (NearlyEqual(mag1, 0.0f) || NearlyEqual(mag2, 0.0f)) {
			return Degrees{ 0.0f };
		}

		float cos{ Dot(target) / std::sqrt(mag1 * mag2) };

		// Clamp cosine to the range [-1, 1] to avoid domain errors for acos. This can very rarely
		// happen due to floating point inaccuracies.
		cos = std::clamp(cos, -1.0f, 1.0f);

		return Radians{ std::acos(cos) }.ToDeg();
	}

	/// @return True if both components are zero (or very close to zero within a small epsilon).
	[[nodiscard]] bool IsZero() const {
		return NearlyEqual(x, T{ 0 }) && NearlyEqual(y, T{ 0 });
	}

	/// @return True if either component are zero (or very close to zero within a small epsilon).
	[[nodiscard]] bool HasZero() const {
		return NearlyEqual(x, T{ 0 }) || NearlyEqual(y, T{ 0 });
	}

	/// @return True if both components are greater than zero. Returns false if either component is
	/// zero (or very close to zero within a small epsilon).
	[[nodiscard]] bool BothAboveZero() const {
		return x > 0 && y > 0 && !HasZero();
	}
};

template <Arithmetic T>
void to_json(json& j, const Vector2<T>& vector);

template <Arithmetic T>
void from_json(const json& j, Vector2<T>& vector);

using V2_int   = Vector2<int>;
using V2_uint  = Vector2<unsigned int>;
using V2_float = Vector2<float>;

template <Arithmetic S>
inline std::ostream& operator<<(std::ostream& os, Vector2<S> v) { // NOSONAR
	os << "(" << v.x << ", " << v.y << ")";
	return os;
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
constexpr Vector2<S> operator+(Vector2<V> lhs, Vector2<U> rhs) { // NOSONAR
	return { static_cast<S>(lhs.x) + static_cast<S>(rhs.x),
			 static_cast<S>(lhs.y) + static_cast<S>(rhs.y) };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
constexpr Vector2<S> operator-(Vector2<V> lhs, Vector2<U> rhs) { // NOSONAR
	return { static_cast<S>(lhs.x) - static_cast<S>(rhs.x),
			 static_cast<S>(lhs.y) - static_cast<S>(rhs.y) };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
constexpr Vector2<S> operator*(Vector2<V> lhs, Vector2<U> rhs) { // NOSONAR
	return { static_cast<S>(lhs.x) * static_cast<S>(rhs.x),
			 static_cast<S>(lhs.y) * static_cast<S>(rhs.y) };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
constexpr Vector2<S> operator/(Vector2<V> lhs, Vector2<U> rhs) { // NOSONAR
	return { static_cast<S>(lhs.x) / static_cast<S>(rhs.x),
			 static_cast<S>(lhs.y) / static_cast<S>(rhs.y) };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
constexpr Vector2<S> operator*(V lhs, Vector2<U> rhs) { // NOSONAR
	return { static_cast<S>(lhs) * static_cast<S>(rhs.x),
			 static_cast<S>(lhs) * static_cast<S>(rhs.y) };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
constexpr Vector2<S> operator*(Vector2<V> lhs, U rhs) { // NOSONAR
	return { static_cast<S>(lhs.x) * static_cast<S>(rhs),
			 static_cast<S>(lhs.y) * static_cast<S>(rhs) };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
constexpr Vector2<S> operator/(V lhs, Vector2<U> rhs) { // NOSONAR
	return { static_cast<S>(lhs) / static_cast<S>(rhs.x),
			 static_cast<S>(lhs) / static_cast<S>(rhs.y) };
}

template <Arithmetic V, Arithmetic U, Arithmetic S = typename std::common_type_t<V, U>>
constexpr Vector2<S> operator/(Vector2<V> lhs, U rhs) { // NOSONAR
	return { static_cast<S>(lhs.x) / static_cast<S>(rhs),
			 static_cast<S>(lhs.y) / static_cast<S>(rhs) };
}

/// @brief Clamp both components of a vector between min and max (component specific).
template <Arithmetic T>
[[nodiscard]] inline Vector2<T> Clamp(Vector2<T> vector, Vector2<T> min, Vector2<T> max) {
	return { std::clamp(vector.x, min.x, max.x), std::clamp(vector.y, min.y, max.y) };
}

/// @brief Clamp the magnitude of the vector between min and max. This means that a (1, 1) vector
/// clamped between -1 and 1 will be (0.7, 0.7)
template <Arithmetic T>
[[nodiscard]] inline Vector2<T> Clamp(Vector2<T> vector, T min, T max) {
	Vector2<T> dir{ vector.Normalized() };
	Vector2<T> dir_min{ dir * Vector2<T>{ min, min } };
	Vector2<T> dir_max{ dir * Vector2<T>{ max, max } };

	Vector2<T> min_v{ std::min(dir_min.x, dir_max.x), std::min(dir_min.y, dir_max.y) };
	Vector2<T> max_v{ std::max(dir_min.x, dir_max.x), std::max(dir_min.y, dir_max.y) };

	return Clamp(vector, min_v, max_v);
}

/// @return True if both the components of a and b are within margin of each other.
template <Arithmetic T>
[[nodiscard]] inline bool WithinMargin(Vector2<T> a, Vector2<T> b, Vector2<T> margin) {
	return std::abs(a.x - b.x) <= margin.x && std::abs(a.y - b.y) <= margin.y;
}

/// @return Ceil both components of a vector.
template <Arithmetic T>
[[nodiscard]] inline Vector2<T> FastCeil(Vector2<T> vector) {
	return { FastCeil(vector.x), FastCeil(vector.y) };
}

/// @return Floor both components of a vector.
template <Arithmetic T>
[[nodiscard]] inline Vector2<T> FastFloor(Vector2<T> vector) {
	return { FastFloor(vector.x), FastFloor(vector.y) };
}

/// @return Round both components of a vector.
template <Arithmetic T>
[[nodiscard]] inline Vector2<T> FastRound(Vector2<T> vector) {
	return { FastRound(vector.x), FastRound(vector.y) };
}

/// @return Absolute value for both components of a vector.
template <Arithmetic T>
[[nodiscard]] inline Vector2<T> Abs(Vector2<T> vector) {
	return { std::abs(vector.x), std::abs(vector.y) };
}

/// @brief Swap both components of vectors a and b.
template <Arithmetic T>
inline void Swap(Vector2<T>& a, Vector2<T>& b) {
	std::swap(a.x, b.x);
	std::swap(a.y, b.y);
}

/// @return Linearly interpolate both components of a vector.
template <Arithmetic T>
[[nodiscard]] inline Vector2<T> Lerp(Vector2<T> lhs, Vector2<T> rhs, T t) {
	return Vector2<T>{ Lerp(lhs.x, rhs.x, t), Lerp(lhs.y, rhs.y, t) };
}

/// @return Linearly interpolate both components of a vector by their respective t values.
template <Arithmetic T>
[[nodiscard]] inline Vector2<T> Lerp(Vector2<T> lhs, Vector2<T> rhs, Vector2<T> t) {
	return Vector2<T>{ Lerp(lhs.x, rhs.x, t.x), Lerp(lhs.y, rhs.y, t.y) };
}

/// @return The midpoint between vectors a and b.
template <Arithmetic T>
[[nodiscard]] inline Vector2<T> Midpoint(Vector2<T> a, Vector2<T> b) {
	return Vector2<T>{ (a + b) / 2.0f };
}

/// @return The larger component of a vector.
template <Arithmetic T>
[[nodiscard]] inline T Max(Vector2<T> vector) {
	return std::max(vector.x, vector.y);
}

/// @return The smaller component of a vector.
template <Arithmetic T>
[[nodiscard]] inline T Min(Vector2<T> vector) {
	return std::min(vector.x, vector.y);
}

[[nodiscard]] bool StrictlyLess(V2_float a, V2_float b, float epsilon = kEpsilon<float>);

template struct Vector2<int>;
template struct Vector2<float>;

} // namespace ptgn

/// @brief Custom hashing function for Vector2 class.
/// This allows for use of unordered maps and sets with Vector2s as keys.
template <ptgn::Arithmetic T>
struct std::hash<ptgn::Vector2<T>> {
	std::size_t operator()(ptgn::Vector2<T> v) const noexcept {
		// Hashing combination algorithm from:
		// https://stackoverflow.com/a/17017281
		std::size_t value{ 17 };
		value = value * 31 + std::hash<T>()(v.x);
		value = value * 31 + std::hash<T>()(v.y);
		return value;
	}
};

namespace ptgn {

template <Arithmetic T>
std::size_t Hash(Vector2<T> vector) {
	return std::hash<Vector2<T>>()(vector);
}

} // namespace ptgn