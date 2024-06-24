#pragma once

#include <chrono>
#include <type_traits>

namespace ptgn {

using hours		   = std::chrono::hours;
using minutes	   = std::chrono::minutes;
using seconds	   = std::chrono::seconds;
using milliseconds = std::chrono::milliseconds;
using microseconds = std::chrono::microseconds;
using nanoseconds  = std::chrono::nanoseconds;

namespace type_traits {

namespace impl {

template <typename T>
struct is_duration : std::false_type {};

template <typename Rep, typename Period>
struct is_duration<std::chrono::duration<Rep, Period>> : std::true_type {};

} // namespace impl

template <typename T>
inline constexpr bool is_duration_v{ impl::is_duration<T>::value };

template <typename T>
using duration = std::enable_if_t<is_duration_v<T>, bool>;

} // namespace type_traits

} // namespace ptgn