#pragma once

#include <chrono>
#include <format>
#include <ostream>
#include <ratio>
#include <regex>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "serialization/json/json.h"

namespace ptgn {

template <typename Rep, typename Period = std::ratio<1>>
using duration = std::chrono::duration<Rep, Period>;

namespace impl {

template <class _Tp>
struct is_chrono_duration : std::false_type {};

template <class _Rep, class _Period>
struct is_chrono_duration<std::chrono::duration<_Rep, _Period>> : std::true_type {};

} // namespace impl

template <typename T>
concept Duration = impl::is_chrono_duration<T>::value;

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

template <Duration To, Duration From>
constexpr To to_duration(const From& duration) {
	return std::chrono::duration_cast<To>(duration);
}

// Generic helper: casts to target duration and returns its count.
template <Duration To, Duration From>
constexpr typename To::rep to_duration_value(const From& duration) {
	return to_duration<To>(duration).count();
}

template <Duration From>
constexpr typename From::rep to_seconds(const From& duration) {
	return to_duration_value<std::chrono::duration<typename From::rep>>(duration);
}

template <Duration From>
constexpr typename From::rep to_milliseconds(const From& duration) {
	return to_duration_value<std::chrono::duration<typename From::rep, std::milli>>(duration);
}

template <Duration From>
constexpr typename From::rep to_microseconds(const From& duration) {
	return to_duration_value<std::chrono::duration<typename From::rep, std::micro>>(duration);
}

template <Duration From>
constexpr typename From::rep to_nanoseconds(const From& duration) {
	return to_duration_value<std::chrono::duration<typename From::rep, std::nano>>(duration);
}

template <Duration From>
constexpr typename From::rep to_minutes(const From& duration) {
	return to_duration_value<std::chrono::duration<typename From::rep, std::ratio<60>>>(duration);
}

template <Duration From>
constexpr typename From::rep to_hours(const From& duration) {
	return to_duration_value<std::chrono::duration<typename From::rep, std::ratio<3600>>>(duration);
}

template <typename Rep, typename Period>
inline std::ostream& operator<<(std::ostream& os, const ptgn::duration<Rep, Period>& d) {
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
struct adl_serializer<ptgn::duration<Rep, Period>> {
	static void to_json(json& j, const ptgn::duration<Rep, Period>& d) {
		using namespace ptgn;
		// Convert duration to milliseconds (common base unit for serialization)
		auto ms{ to_duration<milliseconds>(d) };

		if (ms == d) {
			j = std::to_string(ms.count()) + "ms";
		} else {
			// For non-integral durations (e.g., seconds, minutes)
			float value{ to_duration_value<secondsf>(d) };
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
			throw std::runtime_error("Expected duration as string");
		}

		std::string s{ j.get<std::string>() };
		std::smatch match;

		if (std::regex pattern{ R"(^\s*([\d.]+)\s*(ms|s|min|h)\s*$)", std::regex::icase };
			!std::regex_match(s, match, pattern)) {
			throw std::runtime_error("Invalid duration format: " + s);
		}

		float value{ std::stof(match[1].str()) };
		// Do not make this a string_view, otherwise it may add a \0 to the front.
		std::string unit{ match[2].str() };

		using dur = ptgn::duration<Rep, Period>;

		if (unit == "s" || unit == "S") {
			d = to_duration<dur>(secondsf{ value });
		} else if (unit == "ms" || unit == "MS") {
			d = to_duration<dur>(millisecondsf{ value });
		} else if (unit == "min" || unit == "MIN") {
			d = to_duration<dur>(minutesf{ value });
		} else if (unit == "h" || unit == "H") {
			d = to_duration<dur>(hoursf{ value });
		} else if (unit == "ns" || unit == "NS") {
			d = to_duration<dur>(nanosecondsf{ value });
		} else if (unit == "us" || unit == "US") {
			d = to_duration<dur>(microsecondsf{ value });
		} else {
			throw std::runtime_error("Unsupported time unit: " + std::string(unit));
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