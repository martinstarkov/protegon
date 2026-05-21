#pragma once

#include <chrono>
#include <format>
#include <ostream>
#include <ratio>
#include <regex>
#include <string>
#include <type_traits>

#include "core/log.h"
#include "serialization/json/json.h"

namespace ptgn {

using namespace std::chrono_literals;

template <typename Rep, typename Period = std::ratio<1>>
using duration = std::chrono::duration<Rep, Period>;

namespace impl {

template <class _Tp>
struct is_chrono_duration : std::false_type {};

template <class _Rep, class _Period>
struct is_chrono_duration<std::chrono::duration<_Rep, _Period>> : std::true_type {};

} // namespace impl

template <typename T>
concept DurationType = impl::is_chrono_duration<T>::value;

using hours			= std::chrono::hours;
using hoursf		= duration<float, hours::period>;
using minutes		= std::chrono::minutes;
using minutesf		= duration<float, minutes::period>;
using seconds		= std::chrono::seconds;
using secondsf		= duration<float, seconds::period>;
using milliseconds	= std::chrono::milliseconds;
using millisecondsf = duration<float, milliseconds::period>;
using microseconds	= std::chrono::microseconds;
using microsecondsf = duration<float, microseconds::period>;
using nanoseconds	= std::chrono::nanoseconds;
using nanosecondsf	= duration<float, nanoseconds::period>;

template <typename Rep, typename Period>
std::ostream& operator<<(std::ostream& os, const ptgn::duration<Rep, Period>& d) {
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

template <DurationType T>
[[nodiscard]] constexpr T Lerp(T a, T b, float t) {
	return duration_cast<T>(a + t * (b - a));
}

} // namespace ptgn

NLOHMANN_JSON_NAMESPACE_BEGIN

template <typename Rep, typename Period>
struct adl_serializer<ptgn::duration<Rep, Period>> {
	static void to_json(json& j, const ptgn::duration<Rep, Period>& d) {
		using namespace ptgn;
		// Convert DurationType To milliseconds (common base unit for serialization)
		auto ms{ duration_cast<milliseconds>(d) };

		if (ms == d) {
			j = std::to_string(ms.count()) + "ms";
		} else {
			// For non-integral durations (e.g., seconds, minutes)
			float value{ duration_cast<secondsf>(d).count() };
			if (std::is_integral_v<Rep>) {
				j = std::to_string(ms.count()) + "ms";
			} else {
				j = std::format("{}s", value);
			}
		}
	}

	static void from_json(const json& j, ptgn::duration<Rep, Period>& d) {
		using namespace ptgn;
		if (!j.is_string()) {
			PTGN_ERROR("Expected duration as string");
		}

		std::string s{ j.get<std::string>() };
		std::smatch match;

		// icase = ignore case
		if (std::regex pattern{ R"(^\s*([\d.]+)\s*(ms|s|min|h)\s*$)", std::regex::icase };
			!std::regex_match(s, match, pattern)) {
			PTGN_ERROR("Invalid duration format: ", s);
		}

		float value{ std::stof(match[1].str()) };
		// Do not make this a string_view, otherwise it may add a \0 to the front.
		std::string unit{ match[2].str() };

		using dur = ptgn::duration<Rep, Period>;

		if (unit == "s" || unit == "S") {
			d = duration_cast<dur>(secondsf{ value });
		} else if (unit == "ms" || unit == "MS") {
			d = duration_cast<dur>(millisecondsf{ value });
		} else if (unit == "min" || unit == "MIN") {
			d = duration_cast<dur>(minutesf{ value });
		} else if (unit == "h" || unit == "H") {
			d = duration_cast<dur>(hoursf{ value });
		} else if (unit == "ns" || unit == "NS") {
			d = duration_cast<dur>(nanosecondsf{ value });
		} else if (unit == "us" || unit == "US") {
			d = duration_cast<dur>(microsecondsf{ value });
		} else {
			PTGN_ERROR("Unsupported time unit: ", unit);
		}
	}
};

template <typename Clock, typename Duration>
struct adl_serializer<std::chrono::time_point<Clock, Duration>> {
	static void to_json(json& j, const std::chrono::time_point<Clock, Duration>& tp) {
		j = tp.time_since_epoch().count();
	}

	static void from_json(const json& j, std::chrono::time_point<Clock, Duration>& tp) {
		tp = typename Clock::time_point(ptgn::nanoseconds(j.get<typename Duration::rep>()));
	}
};

NLOHMANN_JSON_NAMESPACE_END