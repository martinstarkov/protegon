#pragma once

#include <cmath>
#include <type_traits>
#include <random>
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
template <typename T>
constexpr const T& Clamp(const T& value, const T& low, const T& high) {
	static_assert(std::is_arithmetic<T>::value, "Clamp can only accept numeric types");
	assert(!(high < low));
	return (value < low) ? low : (high < value) ? high : value;
}

// TODO: Add tests for T being valid integer / supported for numeric limits.
template <typename T>
T Infinity() {
	return std::numeric_limits<T>::infinity();
}

template <typename Floating, std::enable_if_t<std::is_floating_point<Floating>::value, int> = 0>
Floating GetRandomValue(Floating min_range, Floating max_range) {
	std::random_device rd;
	std::mt19937 rng(rd());
	std::uniform_real_distribution<Floating> distribution(min_range, max_range);
	return distribution(rng);
}
template <typename Integer, std::enable_if_t<std::is_integral<Integer>::value, int> = 0>
Integer GetRandomValue(Integer min_range, Integer max_range) {
	std::random_device rd;
	std::mt19937 rng(rd());
	std::uniform_int_distribution<Integer> distribution(min_range, max_range);
	return distribution(rng);
}

static double DegreeToRadian(double degrees) {
	return degrees * PI<double> / 180.0;
}

static double RadianToDegree(double radian) {
	return radian * 180.0 / PI<double>;
}

template <typename ...Ts>
using is_number = std::enable_if_t<(std::is_arithmetic_v<Ts> && ...), int>;

template <typename T, typename S, is_number<T, S> = 0>
inline T RoundCast(S value) {
	return static_cast<T>(std::round(value));
}
template <typename T, is_number<T> = 0>
inline T Round(T value) {
	return static_cast<T>(std::round(value));
}
template <typename T, is_number<T> = 0>
inline T Floor(T value) {
	return static_cast<T>(std::floor(value));
}
template <typename T, is_number<T> = 0>
inline T Ceil(T value) {
	return static_cast<T>(std::ceil(value));
}

// Find the sign of a numeric type
template <typename T>
inline int Sign(T val) {
	static_assert(std::is_arithmetic<T>::value, "Sign function can only accept numeric types");
	return (T(0) < val) - (val < T(0));
}

} // namespace math

} // namespace engine