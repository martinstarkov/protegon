#pragma once

#include <type_traits> // std::enable_if_t, std::is_arithmetic_v, std::is_floating_point_v, etc.

namespace engine {

namespace type_traits {

// Custom template helpers.

// Returns template qualifier of whether or not the type is an integer OR float point number.
// This includes bool, char, char8_t, char16_t, char32_t, wchar_t, short, int, long, long long, float, double, and long double.
template <typename T>
using is_number = std::enable_if_t<std::is_arithmetic_v<T>, bool>;

// Returns template qualifier of whether or not the type is an integer number.
// This includes bool, char, char8_t, char16_t, char32_t, wchar_t, short, int, long, long long.
template <typename T>
using is_integral = std::enable_if_t<std::is_integral_v<T>, bool>;

// Returns template qualifier of whether or not the type is a float point number.
// This includes float, double, and long double.
template <typename T>
using is_floating_point = std::enable_if_t<std::is_floating_point_v<T>, bool>;

// Returns whether or not a type is convertible to another type (double to int, int to float, etc).
template <typename From, typename To>
using convertible = std::enable_if_t<std::is_convertible_v<From, To>, bool>;

} // namespace type_traits

} // namespace engine