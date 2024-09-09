#pragma once

#include <chrono>
#include <ostream>
#include <type_traits>

namespace ptgn {

template <typename Rep, typename Period = std::ratio<1>>
using duration	   = std::chrono::duration<Rep, Period>;
using hours		   = std::chrono::hours;
using minutes	   = std::chrono::minutes;
using seconds	   = std::chrono::seconds;
using milliseconds = std::chrono::milliseconds;
using microseconds = std::chrono::microseconds;
using nanoseconds  = std::chrono::nanoseconds;

namespace tt {

namespace impl {

template <typename T>
struct is_duration : std::false_type {};

template <typename Rep, typename Period>
struct is_duration<ptgn::duration<Rep, Period>> : std::true_type {};

} // namespace impl

template <typename T>
inline constexpr bool is_duration_v{ impl::is_duration<T>::value };

template <typename T>
using duration = std::enable_if_t<is_duration_v<T>, bool>;

} // namespace tt

template <typename T>
std::ostream& operator<<(std::ostream& os, ptgn::duration<T, std::nano> v) {
	return os << v.count() << "ns";
}

template <typename T>
std::ostream& operator<<(std::ostream& os, ptgn::duration<T, std::micro> v) {
	return os << v.count() << "us";
}

template <typename T>
std::ostream& operator<<(std::ostream& os, ptgn::duration<T, std::milli> v) {
	return os << v.count() << "ms";
}

template <typename T>
std::ostream& operator<<(std::ostream& os, ptgn::duration<T, ptgn::seconds::period> v) {
	return os << v.count() << "s";
}

template <typename T>
std::ostream& operator<<(std::ostream& os, ptgn::duration<T, ptgn::minutes::period> v) {
	return os << v.count() << "min";
}

template <typename T>
std::ostream& operator<<(std::ostream& os, ptgn::duration<T, ptgn::hours::period> v) {
	return os << v.count() << "h";
}

} // namespace ptgn