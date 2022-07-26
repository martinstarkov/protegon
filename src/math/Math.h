#pragma once

// This header filer contains standard math functions that are used frequently.
// In some cases with improved performance over standard library alternatives, e.g. ceil and floor.

#include <cstdlib> // std::size_t
#include <cmath> // std::round, std::sqrt, etc
#include <limits> // std::numeric_limits
#include <iomanip> // std::setprecision for truncating
#include <sstream> // std::stringstream for truncating
#include <random> // std::minstd_rand, std::uniform_real_distribution, std::uniform_int_distribution
#include <algorithm> // std::min, std::max
#include <cassert> // assert
#include <type_traits>

namespace ptgn {

namespace type_traits {

namespace internal {

// Comparison operator template helpers

// Source: https://stackoverflow.com/a/44536046
template <typename T, typename U>
using equals_comparison_t = decltype(std::declval<T&>() == std::declval<U&>());

template <typename T, typename U, typename = std::void_t<>>
struct is_equals_comparable : std::false_type {};

template <typename T, typename U>
struct is_equals_comparable<T, U, std::void_t<equals_comparison_t<T, U>>>
    : std::is_same<equals_comparison_t<T, U>, bool> {};

template <typename T, typename U>
using not_equals_comparison_t = decltype(std::declval<T&>() != std::declval<U&>());

template <typename T, typename U, typename = std::void_t<>>
struct is_not_equals_comparable : std::false_type {};

template <typename T, typename U>
struct is_not_equals_comparable<T, U, std::void_t<not_equals_comparison_t<T, U>>>
    : std::is_same<not_equals_comparison_t<T, U>, bool> {};

template <typename T, typename U>
using less_than_comparison_t = decltype(std::declval<T&>() < std::declval<U&>());

template <typename T, typename U, typename = std::void_t<>>
struct is_less_than_comparable : std::false_type {};

template <typename T, typename U>
struct is_less_than_comparable<T, U, std::void_t<less_than_comparison_t<T, U>>>
    : std::is_same<less_than_comparison_t<T, U>, bool> {};

template <typename T, typename U>
using less_than_or_equal_comparison_t = decltype(std::declval<T&>() <= std::declval<U&>());

template <typename T, typename U, typename = std::void_t<>>
struct is_less_than_or_equal_comparable : std::false_type {};

template <typename T, typename U>
struct is_less_than_or_equal_comparable<T, U, std::void_t<less_than_or_equal_comparison_t<T, U>>>
    : std::is_same<less_than_or_equal_comparison_t<T, U>, bool> {};

template <typename T, typename U>
using greater_than_comparison_t = decltype(std::declval<T&>() > std::declval<U&>());

template <typename T, typename U, typename = std::void_t<>>
struct is_greater_than_comparable : std::false_type {};

template <typename T, typename U>
struct is_greater_than_comparable<T, U, std::void_t<greater_than_comparison_t<T, U>>>
    : std::is_same<greater_than_comparison_t<T, U>, bool> {};

template <typename T, typename U>
using greater_than_or_equal_comparison_t = decltype(std::declval<T&>() >= std::declval<U&>());

template <typename T, typename U, typename = std::void_t<>>
struct is_greater_than_or_equal_comparable : std::false_type {};

template <typename T, typename U>
struct is_greater_than_or_equal_comparable<T, U, std::void_t<greater_than_or_equal_comparison_t<T, U>>>
    : std::is_same<greater_than_or_equal_comparison_t<T, U>, bool> {};

} // namespace internal


// Comparison operator template helpers

// True if T and U are comparable using == operator, false otherwise.
template <typename T, typename U>
bool constexpr is_equals_comparable_v{ internal::is_equals_comparable<T, U>::value };

// Template qualifier of whether or not Type is comparable using == operator.
template <typename T, typename U>
using is_equals_comparable_e = std::enable_if_t<is_equals_comparable_v<T, U>, bool>;

// True if T and U are comparable using != operator, false otherwise.
template <typename T, typename U>
bool constexpr is_not_equals_comparable_v{ internal::is_not_equals_comparable<T, U>::value };

// Template qualifier of whether or not Type is comparable using != operator.
template <typename T, typename U>
using is_not_equals_comparable_e = std::enable_if_t<is_not_equals_comparable_v<T, U>, bool>;

// True if T and U are comparable using < operator, false otherwise.
template <typename T, typename U>
bool constexpr is_less_than_comparable_v{ internal::is_less_than_comparable<T, U>::value };

// Template qualifier of whether or not Type is comparable using < operator.
template <typename T, typename U>
using is_less_than_comparable_e = std::enable_if_t<is_less_than_comparable_v<T, U>, bool>;

// True if T and U are comparable using <= operator, false otherwise.
template <typename T, typename U>
bool constexpr is_less_than_or_equal_comparable_v{ internal::is_less_than_or_equal_comparable<T, U>::value };

// Template qualifier of whether or not Type is comparable using <= operator.
template <typename T, typename U>
using is_less_than_or_equal_comparable_e = std::enable_if_t<is_less_than_or_equal_comparable_v<T, U>, bool>;

// True if T and U are comparable using > operator, false otherwise.
template <typename T, typename U>
bool constexpr is_greater_than_comparable_v{ internal::is_greater_than_comparable<T, U>::value };

// Template qualifier of whether or not Type is comparable using > operator.
template <typename T, typename U>
using is_greater_than_comparable_e = std::enable_if_t<is_greater_than_comparable_v<T, U>, bool>;

// True if T and U are comparable using >= operator, false otherwise.
template <typename T, typename U>
bool constexpr is_greater_than_or_equal_comparable_v{ internal::is_greater_than_or_equal_comparable<T, U>::value };

// Template qualifier of whether or not Type is comparable using >= operator.
template <typename T, typename U>
using is_greater_than_or_equal_comparable_e = std::enable_if_t<is_greater_than_or_equal_comparable_v<T, U>, bool>;

}

namespace math {

namespace internal {

template <typename T,
    std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
class Epsilon {};

template<> class Epsilon<float> {
public:
    static constexpr float value() { return 1.0e-5f; }
};

template<> class Epsilon<double> {
public:
    static constexpr double value() { return 1.0e-10; }
};

} // namespace internal

// Definition of PI, requires specification of type (default: double)
// @tparam T - Precision of PI (type: int, float, double, etc).
template <typename T = double, 
    std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
inline const T PI{ std::acos(-T(1)) };

  // General epsilon values for comparing small differences 
// (often used in floating point comparisons).
// TODO: Consider if changing default value to std::numeric_limits<T>::epsilon() is better
template <typename T = double,
    std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
inline constexpr const T EPSILON{ internal::Epsilon<T>::value() };

template <typename T,
    std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
static bool QuadraticFormula(const T& a, const T& b, const T& c, T& out_x0, T& out_x1) {
    const T discriminant{ b * b - static_cast<T>(4) * a * c };
    if (discriminant < static_cast<T>(0)) {
        // Imaginary roots.
        return false;
    } else if (math::Compare(discr, static_cast<T>(0))) {
        // Repeated roots.
        out_x0 = out_x1 = -static_cast<T>(0.5) * b / a;
    } else {
        // Real roots.
        const T q = (b > static_cast<T>(0)) ?
            -static_cast<T>(0.5) * (b + math::Sqrt(discriminant)) :
            -static_cast<T>(0.5) * (b - math::Sqrt(discriminant));
        out_x0 = q / a;
        out_x1 = c / q;
    }
    return true;
}

// Wrapper around std::numeric_limits infinities.
template <typename T, 
    std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
inline constexpr T Infinity() {
    if constexpr (std::is_floating_point_v<T>) {
        return std::numeric_limits<T>::infinity();
    } else if constexpr (std::is_integral_v<T>) {
        return std::numeric_limits<T>::max();
    }
}

// Truncate a double to a specific number of significant figures.
template <typename T, 
    std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
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
    std::enable_if_t<std::is_arithmetic_v<S>, bool> = true,
    std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
inline constexpr S Clamp(T value, T low, T high) {
    assert(high >= low && "Clamp low value must be below or equal to high value");
    return static_cast<S>((value < low) ? low : (high < value) ? high : value);
}

// Clamp value within a range from low to high.
template <typename T, 
    std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
inline constexpr T Clamp(T value, T low, T high) {
    assert(high >= low && "Clamp low value must be below or equal to high value");
    return (value < low) ? low : (high < value) ? high : value;
}

// Convert degrees to radians.
template <typename T = double, 
    std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
inline T DegreesToRadians(T degrees) {
	return degrees * PI<T> / static_cast<T>(180.0);
}

// Convert radians to degrees.
template <typename T = double,
    std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
inline T RadiansToDegrees(T degrees) {
    return degrees * static_cast<T>(180.0) / PI<T>;
}

// Returns the maximum of two values, if equal first value is returned.
template <typename T, typename U, 
    type_traits::is_greater_than_comparable_e<T, U> = true>
inline auto Max(const T& left, const U& right) {
    return right > left ? right : left;
}

// Returns the minimum of two values, if equal first value is returned.
template <typename T, typename U,
    type_traits::is_less_than_comparable_e<T, U> = true>
inline auto Min(const T& left, const U& right) {
    return right < left ? right : left;
}

template <typename T = double,
    std::enable_if_t<std::is_integral_v<T>, bool> = true>
inline T Compare(T x, T y, T relative_tolerance = 0, T absolute_tolerance = 0) {
    return x == y;
}

// Compare two floating point numbers using relative tolerance and absolute tolerances.
// The absolute tolerance test fails when x and y become large.
// The relative tolerance test fails when x and y become small.
template <typename T = double,
    std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
inline T Compare(T x, T y, T relative_tolerance, T absolute_tolerance) {
    return Abs(x - y) <= Max(absolute_tolerance, relative_tolerance * Max(Abs(x), Abs(y)));
}

// Compare two floating point numbers using equal relative and absolute tolerances.
// The absolute tolerance test fails when x and y become large.
// The relative tolerance test fails when x and y become small.
template <typename T = double,
    std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
inline T Compare(T x, T y, T tolerance = EPSILON<T>) {
    return Compare(x, y, tolerance, tolerance);
}

// Signum function.
// Returns  1  if value is positive.
// Returns  0  if value is zero.
// Returns -1  if value is negative.
template <typename T, 
    std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
inline T Sign(T value) {
    return static_cast<T>((T(0) < value) - (value < T(0)));
}

// With template modifications to https://stackoverflow.com/a/30308919.

// Faster alternative to std::floor for floating point numbers.
template <typename T = int, typename U, 
    std::enable_if_t<std::is_arithmetic_v<T>, bool> = true,
    std::enable_if_t<std::is_arithmetic_v<U>, bool> = true>
inline T Floor(U value) {
    if constexpr (std::is_integral_v<U>) {
        return static_cast<T>(value);
    } else if  constexpr (std::is_floating_point_v<U>) {
        return static_cast<T>((int)value - (value < (int)value));
    }
}

// Faster alternative to std::ceil for floating point numbers.
template <typename T = int, typename U, 
    std::enable_if_t<std::is_arithmetic_v<T>, bool> = true,
    std::enable_if_t<std::is_arithmetic_v<U>, bool> = true>
inline T Ceil(U value) {
    if constexpr (std::is_integral_v<U>) {
        return static_cast<T>(value);
    } else if  constexpr (std::is_floating_point_v<U>) {
        return static_cast<T>((int)value + (value > (int)value));
    }
}

// Currently the same as std::round. Possible to change in the future if necessary.
template <typename T = int, typename U, 
    std::enable_if_t<std::is_arithmetic_v<T>, bool> = true,
    std::enable_if_t<std::is_arithmetic_v<U>, bool> = true>
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
    std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
inline T Abs(T value) {
    return (value >= 0) ? value : -value;
}

// Currently the same as std::sqrt. Possible to change in the future if necessary.
template <typename T = double, typename U, 
    std::enable_if_t<std::is_arithmetic_v<T>, bool> = true,
    std::enable_if_t<std::is_arithmetic_v<U>, bool> = true>
inline T Sqrt(U value) {
    return static_cast<T>(std::sqrt(value));
}

// Linearly interpolate between two values by a given amount.
// Allows for casting to specific type.
template <typename S, typename T, typename U, 
    std::enable_if_t<std::is_arithmetic_v<S>, bool> = true,
    std::enable_if_t<std::is_arithmetic_v<T>, bool> = true,
    std::enable_if_t<std::is_floating_point_v<U>, bool> = true>
inline S Lerp(T a, T b, U t) {
    return static_cast<S>(static_cast<U>(a) + t * static_cast<U>(b - a));
}

// Linearly interpolate between two values by a given amount.
template <typename U, typename T, 
    std::enable_if_t<std::is_arithmetic_v<T>, bool> = true,
    std::enable_if_t<std::is_floating_point_v<U>, bool> = true>
inline U Lerp(T a, T b, U t) {
    return static_cast<U>(a) + t * static_cast<U>(b - a);
}

template <typename T, 
    std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
inline T SmoothStep(const T& value) {
    return value * value * ((T)3 - (T)2 * value);
}

} // namespace math

} // namespace ptgn