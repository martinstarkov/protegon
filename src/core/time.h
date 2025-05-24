#pragma once

#include <chrono>
#include <iosfwd>
#include <ratio>
#include <type_traits>

#include "serialization/json.h"

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

template <typename Rep, typename Period>
std::ostream& operator<<(std::ostream& os, const std::chrono::duration<Rep, Period>& d) {
	os << d.count();

	if constexpr (std::is_same_v<Period, std::milli>) {
		os << " ms";			// Milliseconds
	} else if constexpr (std::is_same_v<Period, std::micro>) {
		os << " us";			// Microseconds
	} else if constexpr (std::is_same_v<Period, std::nano>) {
		os << " ns";			// Nanoseconds
	} else if constexpr (std::is_same_v<Period, std::ratio<1>>) {
		os << " s";				// Seconds
	} else if constexpr (std::is_same_v<Period, std::ratio<60>>) {
		os << " min";			// Minutes
	} else if constexpr (std::is_same_v<Period, std::ratio<3600>>) {
		os << " h";				// Hours
	} else {
		os << " [custom unit]"; // Fallback for other units
	}

	return os;
}

} // namespace ptgn

NLOHMANN_JSON_NAMESPACE_BEGIN

template <typename Rep, typename Period>
struct adl_serializer<std::chrono::duration<Rep, Period>> {
	static void to_json(json& j, const std::chrono::duration<Rep, Period>& d) {
		j = std::chrono::duration_cast<std::chrono::duration<double>>(d).count();
	}

	static void from_json(const json& j, std::chrono::duration<Rep, Period>& d) {
		d = std::chrono::duration<Rep, Period>(
			static_cast<Rep>(j.get<double>() / Period::num * Period::den)
		);
	}
};

template <typename Clock, typename Duration>
struct adl_serializer<std::chrono::time_point<Clock, Duration>> {
	static void to_json(json& j, const std::chrono::time_point<Clock, Duration>& tp) {
		j = tp.time_since_epoch().count();
	}

	static void from_json(const json& j, std::chrono::time_point<Clock, Duration>& tp) {
		tp = typename Clock::time_point(std::chrono::nanoseconds(j.get<typename Duration::rep>()));
	}
};

NLOHMANN_JSON_NAMESPACE_END