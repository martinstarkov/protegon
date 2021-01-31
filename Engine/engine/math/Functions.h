#pragma once

// This header filer contains standard math functions that are used frequently.
// In some cases with improved performance over standard library alternatives, e.g. ceil and floor.

#include <cassert> // assert
#include <cmath> // std::round, std::sqrt
#include <limits> // std::numeric_limits
#include <iomanip> // std::setprecision for truncating
#include <sstream> // std::stringstream for truncating
#include <random> // std::minstd_rand, std::uniform_real_distribution, std::uniform_int_distribution

#include "utils/TypeTraits.h"

namespace engine {

namespace math {

template <typename T, type_traits::is_number<T> = true>
T const PI = std::acos(-T(1));

template <typename T, type_traits::is_number<T> = true>
constexpr T Infinity() {
    if constexpr (std::is_floating_point_v<T>) {
        return std::numeric_limits<T>::infinity();
    } else if constexpr (std::is_integral_v<T>) {
        return std::numeric_limits<T>::max();
    }
}

// Return a random number in the given range.
template <typename T, type_traits::is_number<T> = true>
static T Random(T min = T{ 0 }, T max = T{ 1 }) {
    assert(min < max && "Minimum random value must be less than maximum random value");
    std::minstd_rand gen(std::random_device{}());
    // Vary distribution type based on template parameter type.
    if constexpr (std::is_floating_point_v<T>) {
        std::uniform_real_distribution<T> dist(min, max);
        return dist(gen);
    } else if constexpr (std::is_integral_v<T>) {
        std::uniform_int_distribution<T> dist(min, max);
        return dist(gen);
    }
}

// Truncate a double to a specific number of significant figures.
template <typename T, type_traits::is_floating_point<T> = true>
static T Truncate(T value, int significant_figures) {
    assert(significant_figures >= 0 && "Cannot truncate to a negative number of significant figures");
	std::stringstream stream;
	stream << std::fixed << std::setprecision(significant_figures) << value;
    // Convert stringstream to correct floating point type based on template parameter.
    if constexpr (std::is_same_v<T, float>) {
        return std::stof(stream.str());
    } else if constexpr (std::is_same_v<T, double>) {
        return std::stod(stream.str());
    } else if constexpr (std::is_same_v<T, long double>) {
        return std::stold(stream.str());
    }
}

// Clamp value within a range from low to high.
template <typename T, type_traits::is_number<T> = true>
constexpr T Clamp(T value, T low, T high) {
	assert(high >= low && "Clamp low value must be below or equal to high value");
	return (value < low) ? low : (high < value) ? high : value;
}

// Convert degrees to radians.
inline double DegreesToRadians(double degrees) {
	return degrees * PI<double> / 180.0;
}

// Convert radians to degrees.
inline double RadiansToDegrees(double degrees) {
    return degrees * 180.0 / PI<double>;
}

// Signum function.
// Returns  1  if value is positive.
// Returns  0  if value is zero.
// Returns -1  if value is negative.
template <typename T, type_traits::is_number<T> = true>
inline T Sign(T value) {
    return static_cast<T>((T(0) < value) - (value < T(0)));
}

// With template modifications to https://stackoverflow.com/a/30308919.

// Faster alternative to std::floor for floating point numbers.
template <typename T = int, typename U, type_traits::is_number<T> = true, type_traits::is_number<U> = true>
inline T Floor(U value) {
    if constexpr (std::is_integral_v<U>) {
        return static_cast<T>(value);
    } else if  constexpr (std::is_floating_point_v<U>) {
        return static_cast<T>((int)value - (value < (int)value));
    }
}

// Faster alternative to std::ceil for floating point numbers.
template <typename T = int, typename U, type_traits::is_number<T> = true, type_traits::is_number<U> = true>
inline T Ceil(U value) {
    if constexpr (std::is_integral_v<U>) {
        return static_cast<T>(value);
    } else if  constexpr (std::is_floating_point_v<U>) {
        return static_cast<T>((int)value + (value > (int)value));
    }
}

// Currently the same as std::round. Possible to change in the future if necessary.
template <typename T = int, typename U, type_traits::is_number<T> = true, type_traits::is_number<U> = true>
inline T Round(U value) {
    if constexpr (std::is_integral_v<U>) {
        return static_cast<T>(value);
    } else if  constexpr (std::is_floating_point_v<U>) {
        return static_cast<T>(std::round(value));
    }
}

// Faster alternative to std::abs.
// Not to be confused with workout plans.
template <typename T, type_traits::is_number<T> = true>
inline T Abs(T value) {
    return (value >= 0) ? value : -value;
}

// Currently the same as std::sqrt. Possible to change in the future if necessary.
template <typename T = double, typename U, type_traits::is_number<T> = true, type_traits::is_number<U> = true>
inline T Sqrt(U value) {
    return static_cast<T>(std::sqrt(value));
}

// Linearly interpolate between two values by a given amount.
// Allows for casting to specific type.
template <typename S, typename T, typename U, type_traits::is_number<T> = true, type_traits::is_number<S> = true, type_traits::is_floating_point<U> = true>
inline S Lerp(T a, T b, U amount) {
    // TODO: Write some tests for this?
    return static_cast<S>(static_cast<U>(a) + amount * static_cast<U>(b - a));
}

// Linearly interpolate between two values by a given amount.
template <typename U, typename T, type_traits::is_number<T> = true, type_traits::is_floating_point<U> = true>
inline U Lerp(T a, T b, U amount) {
    // TODO: Write some tests for this?
    return static_cast<U>(a) + amount * static_cast<U>(b - a);
}

} // namespace math

} // namespace engine