#pragma once

#include <chrono>
#include <iosfwd>
#include <ratio>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
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
		// Convert duration to milliseconds (common base unit for serialization)
		auto ms{ std::chrono::duration_cast<ptgn::milliseconds>(d) };

		if (ms == d) {
			j = std::to_string(ms.count()) + "ms";
		} else {
			// For non-integral durations (e.g., seconds, minutes)
			double value = std::chrono::duration_cast<ptgn::duration<double>>(d).count();
			if (std::is_integral_v<Rep>) {
				j = std::to_string(ms.count()) + "ms";
			} else {
				j = std::to_string(value) + "s"; // Default fallback
			}
		}
	}

	static void from_json(const json& j, std::chrono::duration<Rep, Period>& d) {
		if (!j.is_string()) {
			throw std::runtime_error("Expected duration as string");
		}

		std::string s{ j.get<std::string>() };
		std::smatch match;
		std::regex pattern(R"(^\s*([\d.]+)\s*(ms|s|min|h)\s*$)", std::regex::icase);

		if (!std::regex_match(s, match, pattern)) {
			throw std::runtime_error("Invalid duration format: " + s);
		}

		double value{ std::stod(match[1].str()) };
		// Do not make this a string_view, otherwise it may add a \0 to the front.
		std::string unit{ match[2].str() };

		using dur = std::chrono::duration<Rep, Period>;

		if (unit == "s" || unit == "S") {
			d = std::chrono::duration_cast<dur>(ptgn::duration<double, ptgn::seconds::period>(value)
			);
		} else if (unit == "ms" || unit == "MS") {
			d = std::chrono::duration_cast<dur>(
				ptgn::duration<double, ptgn::milliseconds::period>(value)
			);
		} else if (unit == "min" || unit == "MIN") {
			d = std::chrono::duration_cast<dur>(ptgn::duration<double, ptgn::minutes::period>(value)
			);
		} else if (unit == "h" || unit == "H") {
			d = std::chrono::duration_cast<dur>(ptgn::duration<double, ptgn::hours::period>(value));
		} else if (unit == "ns" || unit == "NS") {
			d = std::chrono::duration_cast<dur>(
				ptgn::duration<double, ptgn::nanoseconds::period>(value)
			);
		} else if (unit == "us" || unit == "US") {
			d = std::chrono::duration_cast<dur>(
				ptgn::duration<double, ptgn::microseconds::period>(value)
			);
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
		tp = typename Clock::time_point(std::chrono::nanoseconds(j.get<typename Duration::rep>()));
	}
};

NLOHMANN_JSON_NAMESPACE_END