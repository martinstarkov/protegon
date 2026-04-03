#pragma once

#include <ostream>

#include "core/math/math_utils.h"
#include "core/math/tolerance.h"
#include "serialization/json/serialize.h"

namespace ptgn {

struct Radians;

struct Degrees {
	Degrees() = default;

	constexpr Degrees(float value) : value{ value } {} // NOSONAR

	explicit constexpr Degrees(Radians r);

	[[nodiscard]] constexpr Radians ToRad() const;

	[[nodiscard]] float Tan() const;

	[[nodiscard]] float Sin() const;

	[[nodiscard]] float Cos() const;

	/// @return Random angle in the range [0.0, 360.0].
	[[nodiscard]] static Degrees Random();

	friend bool operator==(const Degrees& a, const Degrees& b) {
		return NearlyEqual(a.value, b.value);
	}

	friend std::ostream& operator<<(std::ostream& os, Degrees d) {
		os << d.value << " deg";
		return os;
	}

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Degrees, value)

	float value{ 0.0f };
};

struct Radians {
	Radians() = default;

	explicit constexpr Radians(float value) : value{ value } {}

	explicit constexpr Radians(Degrees d);

	[[nodiscard]] constexpr Degrees ToDeg() const;

	[[nodiscard]] float Tan() const;

	[[nodiscard]] float Sin() const;

	[[nodiscard]] float Cos() const;

	/// @return Random angle in the range [0.0, kTwoPi].
	[[nodiscard]] static Radians Random();

	friend bool operator==(const Radians& a, const Radians& b) {
		return NearlyEqual(a.value, b.value);
	}

	friend std::ostream& operator<<(std::ostream& os, Radians r) {
		os << r.ToDeg();
		return os;
	}

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Radians, value)

	float value{ 0.0f };
};

constexpr Degrees::Degrees(Radians r) : value{ r.value * 180.0f / kPi } {}

constexpr Radians::Radians(Degrees d) : value{ d.value * kPi / 180.0f } {}

constexpr Radians Degrees::ToRad() const {
	return Radians{ *this };
}

constexpr Degrees Radians::ToDeg() const {
	return Degrees{ *this };
}

consteval Degrees operator"" _deg(long double d) {
	return Degrees{ static_cast<float>(d) };
}

consteval Radians operator"" _rad(long double r) {
	return Radians{ static_cast<float>(r) };
}

constexpr Degrees operator+(Degrees a, Degrees b) { // NOSONAR
	return Degrees{ a.value + b.value };
}

constexpr Degrees operator-(Degrees a, Degrees b) { // NOSONAR
	return Degrees{ a.value - b.value };
}

constexpr Degrees operator*(Degrees a, float s) { // NOSONAR
	return Degrees{ a.value * s };
}

constexpr Degrees operator*(float s, Degrees a) { // NOSONAR
	return a * s;
}

constexpr Degrees operator/(Degrees a, float s) { // NOSONAR
	return Degrees{ a.value / s };
}

constexpr Degrees& operator+=(Degrees& a, Degrees b) {
	a.value += b.value;
	return a;
}

constexpr Degrees& operator-=(Degrees& a, Degrees b) {
	a.value -= b.value;
	return a;
}

constexpr Degrees& operator*=(Degrees& a, float s) {
	a.value *= s;
	return a;
}

constexpr Degrees& operator/=(Degrees& a, float s) {
	a.value /= s;
	return a;
}

constexpr Degrees operator-(Degrees a) {
	return Degrees{ -a.value };
}

constexpr Radians operator+(Radians a, Radians b) { // NOSONAR
	return Radians{ a.value + b.value };
}

constexpr Radians operator-(Radians a, Radians b) { // NOSONAR
	return Radians{ a.value - b.value };
}

constexpr Radians operator*(Radians a, float s) { // NOSONAR
	return Radians{ a.value * s };
}

constexpr Radians operator*(float s, Radians a) { // NOSONAR
	return a * s;
}

constexpr Radians operator/(Radians a, float s) { // NOSONAR
	return Radians{ a.value / s };
}

constexpr Radians& operator+=(Radians& a, Radians b) {
	a.value += b.value;
	return a;
}

constexpr Radians& operator-=(Radians& a, Radians b) {
	a.value -= b.value;
	return a;
}

constexpr Radians& operator*=(Radians& a, float s) {
	a.value *= s;
	return a;
}

constexpr Radians& operator/=(Radians& a, float s) {
	a.value /= s;
	return a;
}

constexpr Radians operator-(Radians a) {
	return Radians{ -a.value };
}

/// @brief Clamp angle in degrees from [0, 360).
[[nodiscard]] Degrees Clamp(Degrees d);

/// @return Clamp angle in radians in range [0, 2 pi).
[[nodiscard]] Radians Clamp(Radians r);

/// @brief Linearly interpolate between a and b by t.
[[nodiscard]] constexpr Degrees Lerp(Degrees a, Degrees b, float t) {
	return a + t * (b - a);
}

/// @brief Linearly interpolate between a and b by t.
[[nodiscard]] constexpr Radians Lerp(Radians a, Radians b, float t) {
	return a + t * (b - a);
}

} // namespace ptgn