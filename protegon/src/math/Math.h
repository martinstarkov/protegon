#pragma once

// This header filer contains standard math functions that are used frequently.
// In some cases with improved performance over standard library alternatives, e.g. ceil and floor.

#include <cassert> // assert
#include <cmath> // std::round, std::sqrt, etc
#include <limits> // std::numeric_limits
#include <iomanip> // std::setprecision for truncating
#include <sstream> // std::stringstream for truncating
#include <random> // std::minstd_rand, std::uniform_real_distribution, std::uniform_int_distribution

#include "utils/TypeTraits.h"

namespace engine {

namespace math {

// Definition of PI
// @tparam T - Precision of PI (type: int, float, double, etc).
template <typename T, 
    type_traits::is_number_e<T> = true>
inline T const PI{ std::acos(-T(1)) };

// Wrapper around std::numeric_limits infinities.
template <typename T, 
    type_traits::is_number_e<T> = true>
inline constexpr T Infinity() {
    if constexpr (std::is_floating_point_v<T>) {
        return std::numeric_limits<T>::infinity();
    } else if constexpr (std::is_integral_v<T>) {
        return std::numeric_limits<T>::max();
    }
}

// Truncate a double to a specific number of significant figures.
template <typename T, 
    type_traits::is_floating_point_e<T> = true>
inline T Truncate(T value, int significant_figures) {
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
// Allows for static cast.
template <typename S, typename T,
    type_traits::is_number_e<S> = true,
    type_traits::is_number_e<T> = true>
inline constexpr S Clamp(T value, T low, T high) {
    assert(high >= low && "Clamp low value must be below or equal to high value");
    return static_cast<S>((value < low) ? low : (high < value) ? high : value);
}

// Clamp value within a range from low to high.
template <typename T, 
    type_traits::is_number_e<T> = true>
inline constexpr T Clamp(T value, T low, T high) {
    assert(high >= low && "Clamp low value must be below or equal to high value");
    return (value < low) ? low : (high < value) ? high : value;
}

// Convert degrees to radians.
template <typename T = double, 
    type_traits::is_floating_point_e<T> = true>
inline T DegreesToRadians(T degrees) {
	return degrees * PI<T> / static_cast<T>(180.0);
}

// Convert radians to degrees.
template <typename T = double,
    type_traits::is_floating_point_e<T> = true>
inline T RadiansToDegrees(T degrees) {
    return degrees * static_cast<T>(180.0) / PI<T>;
}

// Signum function.
// Returns  1  if value is positive.
// Returns  0  if value is zero.
// Returns -1  if value is negative.
template <typename T, 
    type_traits::is_number_e<T> = true>
inline T Sign(T value) {
    return static_cast<T>((T(0) < value) - (value < T(0)));
}

// With template modifications to https://stackoverflow.com/a/30308919.

// Faster alternative to std::floor for floating point numbers.
template <typename T = int, typename U, 
    type_traits::is_number_e<T> = true,
    type_traits::is_number_e<U> = true>
inline T Floor(U value) {
    if constexpr (std::is_integral_v<U>) {
        return static_cast<T>(value);
    } else if  constexpr (std::is_floating_point_v<U>) {
        return static_cast<T>((int)value - (value < (int)value));
    }
}

// Faster alternative to std::ceil for floating point numbers.
template <typename T = int, typename U, 
    type_traits::is_number_e<T> = true,
    type_traits::is_number_e<U> = true>
inline T Ceil(U value) {
    if constexpr (std::is_integral_v<U>) {
        return static_cast<T>(value);
    } else if  constexpr (std::is_floating_point_v<U>) {
        return static_cast<T>((int)value + (value > (int)value));
    }
}

// Currently the same as std::round. Possible to change in the future if necessary.
template <typename T = int, typename U, 
    type_traits::is_number_e<T> = true,
    type_traits::is_number_e<U> = true>
inline T Round(U value) {
    if constexpr (std::is_integral_v<U>) {
        return static_cast<T>(value);
    } else if  constexpr (std::is_floating_point_v<U>) {
        return static_cast<T>(std::round(value));
    }
}

// Faster alternative to std::abs.
// Not to be confused with workout plans.
template <typename T, 
    type_traits::is_number_e<T> = true>
inline T Abs(T value) {
    return (value >= 0) ? value : -value;
}

// Currently the same as std::sqrt. Possible to change in the future if necessary.
template <typename T = double, typename U, 
    type_traits::is_number_e<T> = true,
    type_traits::is_number_e<U> = true>
inline T Sqrt(U value) {
    return static_cast<T>(std::sqrt(value));
}

// Linearly interpolate between two values by a given amount.
// Allows for casting to specific type.
template <typename S, typename T, typename U, 
    type_traits::is_number_e<S> = true,
    type_traits::is_number_e<T> = true,
    type_traits::is_floating_point_e<U> = true>
inline S Lerp(T a, T b, U t) {
    return static_cast<S>(static_cast<U>(a) + t * static_cast<U>(b - a));
}

// Linearly interpolate between two values by a given amount.
template <typename U, typename T, 
    type_traits::is_number_e<T> = true,
    type_traits::is_floating_point_e<U> = true>
inline U Lerp(T a, T b, U t) {
    return static_cast<U>(a) + t * static_cast<U>(b - a);
}

template <typename T, 
    type_traits::is_number_e<T> = true>
inline T SmoothStep(const T& value) {
    return value * value * ((T)3 - (T)2 * value);
}

} // namespace math

} // namespace engine