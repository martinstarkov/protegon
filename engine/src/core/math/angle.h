#pragma once

#include <cmath>
#include <ostream>

#include "core/math/math_utils.h"
#include "core/math/tolerance.h"
#include "serialization/json/fwd.h"

namespace ptgn {

struct Radians;

struct Degrees {
	Degrees() = default;

	constexpr Degrees(float value) : value{ value } {} // NOSONAR

	explicit constexpr Degrees(Radians r);

	[[nodiscard]] constexpr Radians ToRad() const;

	[[nodiscard]] constexpr float Tan() const;

	[[nodiscard]] constexpr float Sin() const;

	[[nodiscard]] constexpr float Cos() const;

	/// @return Random angle in the range [0.0, 360.0].
	[[nodiscard]] static Degrees Random();

	/// @return Random angle in the range [min, max].
	[[nodiscard]] static Degrees Random(Degrees min, Degrees max);

	constexpr friend bool operator==(const Degrees& a, const Degrees& b) {
		return NearlyEqual(a.value, b.value);
	}

	friend std::ostream& operator<<(std::ostream& os, Degrees d) {
		os << d.value << " deg";
		return os;
	}

	float value{ 0.0f };
};

struct Radians {
	Radians() = default;

	explicit constexpr Radians(float value) : value{ value } {}

	explicit constexpr Radians(Degrees d) : value{ d.value * kPi / 180.0f } {}

	[[nodiscard]] constexpr Degrees ToDeg() const {
		return Degrees{ *this };
	}

	[[nodiscard]] constexpr float Tan() const {
		return std::tan(value);
	}

	[[nodiscard]] constexpr float Sin() const {
		return std::sin(value);
	}

	[[nodiscard]] constexpr float Cos() const {
		return std::cos(value);
	}

	/// @return Random angle in the range [0.0, kTwoPi].
	[[nodiscard]] static Radians Random();

	/// @return Random angle in the range [min, max].
	[[nodiscard]] static Radians Random(Radians min, Radians max);

	constexpr friend bool operator==(const Radians& a, const Radians& b) {
		return NearlyEqual(a.value, b.value);
	}

	friend std::ostream& operator<<(std::ostream& os, Radians r) {
		os << r.value << " rad";
		return os;
	}

	float value{ 0.0f };
};

constexpr Degrees::Degrees(Radians r) : value{ r.value * 180.0f / kPi } {}

constexpr Radians Degrees::ToRad() const {
	return Radians{ *this };
}

constexpr float Degrees::Tan() const {
	return ToRad().Tan();
}

constexpr float Degrees::Sin() const {
	return ToRad().Sin();
}

constexpr float Degrees::Cos() const {
	return ToRad().Cos();
}

void to_json(json& j, const Degrees& angle);
void from_json(const json& j, Degrees& angle);

void to_json(json& j, const Radians& angle);
void from_json(const json& j, Radians& angle);

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
[[nodiscard]] constexpr Degrees Clamp(Degrees d) {
	float clamped{ std::fmod(d.value, 360.0f) };

	if (clamped < 0.0f) {
		clamped += 360.0f;
	}

	return Degrees{ clamped };
}

/// @return Clamp angle in radians in range [0, 2 pi).
[[nodiscard]] constexpr Radians Clamp(Radians r) {
	float clamped{ std::fmod(r.value, kTwoPi) };

	if (clamped < 0.0f) {
		clamped += kTwoPi;
	}

	return Radians{ clamped };
}

/// @brief Linearly interpolate between a and b by t.
[[nodiscard]] constexpr Degrees Lerp(Degrees a, Degrees b, float t) {
	return a + t * (b - a);
}

/// @brief Linearly interpolate between a and b by t.
[[nodiscard]] constexpr Radians Lerp(Radians a, Radians b, float t) {
	return a + t * (b - a);
}

} // namespace ptgn