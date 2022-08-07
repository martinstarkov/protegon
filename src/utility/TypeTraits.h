#pragma once

#include <type_traits>

namespace ptgn {

namespace type_traits {

namespace impl {

// Comparison operator template helpers.

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

} // namespace impl

// Comparison operator template helpers

// True if T and U are comparable using == operator, false otherwise.
template <typename T, typename U>
bool constexpr is_equals_comparable_v{ impl::is_equals_comparable<T, U>::value };

// Template qualifier of whether or not Type is comparable using == operator.
template <typename T, typename U>
using is_equals_comparable_e = std::enable_if_t<is_equals_comparable_v<T, U>, bool>;

// True if T and U are comparable using != operator, false otherwise.
template <typename T, typename U>
bool constexpr is_not_equals_comparable_v{ impl::is_not_equals_comparable<T, U>::value };

// Template qualifier of whether or not Type is comparable using != operator.
template <typename T, typename U>
using is_not_equals_comparable_e = std::enable_if_t<is_not_equals_comparable_v<T, U>, bool>;

// True if T and U are comparable using < operator, false otherwise.
template <typename T, typename U>
bool constexpr is_less_than_comparable_v{ impl::is_less_than_comparable<T, U>::value };

// Template qualifier of whether or not Type is comparable using < operator.
template <typename T, typename U>
using is_less_than_comparable_e = std::enable_if_t<is_less_than_comparable_v<T, U>, bool>;

// True if T and U are comparable using <= operator, false otherwise.
template <typename T, typename U>
bool constexpr is_less_than_or_equal_comparable_v{ impl::is_less_than_or_equal_comparable<T, U>::value };

// Template qualifier of whether or not Type is comparable using <= operator.
template <typename T, typename U>
using is_less_than_or_equal_comparable_e = std::enable_if_t<is_less_than_or_equal_comparable_v<T, U>, bool>;

// True if T and U are comparable using > operator, false otherwise.
template <typename T, typename U>
bool constexpr is_greater_than_comparable_v{ impl::is_greater_than_comparable<T, U>::value };

// Template qualifier of whether or not Type is comparable using > operator.
template <typename T, typename U>
using is_greater_than_comparable_e = std::enable_if_t<is_greater_than_comparable_v<T, U>, bool>;

// True if T and U are comparable using >= operator, false otherwise.
template <typename T, typename U>
bool constexpr is_greater_than_or_equal_comparable_v{ impl::is_greater_than_or_equal_comparable<T, U>::value };

// Template qualifier of whether or not Type is comparable using >= operator.
template <typename T, typename U>
using is_greater_than_or_equal_comparable_e = std::enable_if_t<is_greater_than_or_equal_comparable_v<T, U>, bool>;

} // namespace type_traits

} // namespace ptgn