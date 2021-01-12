#pragma once

#include <cmath> // std::round
#include <type_traits> // std::enable_if_t, std::is_arithmetic_v, std::is_floating_point_v
#include <limits> // std::numeric_limits
#include <iomanip> // std::setprecision for truncating
#include <sstream> // std::stringstream for truncating

namespace internal {

} // namespace internal

namespace engine {

namespace math {

template<typename T>
T const PI = std::acos(-T(1));

// Truncate to specific amount of significant figures
inline double Truncate(double value, int digits) {
	std::stringstream stream;
	stream << std::fixed << std::setprecision(digits) << value;
	return std::stod(stream.str());
}

// Clamp value within a range.
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
constexpr T Clamp(T value, T low, T high) {
	assert(!(high < low));
	return (value < low) ? low : (high < value) ? high : value;
}

static double DegreeToRadian(double degrees) {
	return degrees * PI<double> / 180.0;
}

static double RadianToDegree(double radian) {
	return radian * 180.0 / PI<double>;
}

template <typename T>
static int Sign(T value) {
    return (T(0) < value) - (value < T(0));
}

// With template modifications to https://stackoverflow.com/a/30308919.

// Faster alternative to std::floor for floating point numbers.
template <typename T, std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
static int FastFloor(T value) {
    return (int)value - (value < (int)value);
}

// If called on integer types, return the value.
template <typename T, std::enable_if_t<std::is_integral_v<T>, bool> = true>
static T FastFloor(T value) {
    return value;
}

// Faster alternative to std::ceil for floating point numbers.
template <typename T, std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
static int FastCeil(T value) {
    return (int)value + (value > (int)value);
}

// If called on integer types, return the value.
template <typename T, std::enable_if_t<std::is_integral_v<T>, bool> = true>
static T FastCeil(T value) {
    return value;
}

// Faster alternative to std::abs.
// Not to be confused with workout plans.
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
static T FastAbs(T value) {
    return (value >= 0) ? value : -value;
}

// Currently the same as std::round. Possible to change in the future if necessary.
template <typename T, std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
static T FastRound(T value) {
    return std::round(value);
}

// If called on integer types, return the value.
template <typename T, std::enable_if_t<std::is_integral_v<T>, bool> = true>
static T FastRound(T value) {
    return value;
}

} // namespace math

} // namespace engine