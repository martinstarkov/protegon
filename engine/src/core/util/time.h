#pragma once

#include <algorithm>
#include <chrono>
#include <cctype>
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

template <typename T>
struct is_chrono_duration : std::false_type {};

template <typename Rep, typename Period>
struct is_chrono_duration<std::chrono::duration<Rep, Period>> : std::true_type {};

} // namespace impl

template <typename T>
concept DurationType = impl::is_chrono_duration<std::remove_cvref_t<T>>::value;

using weeks          = std::chrono::weeks;
using weeksf         = duration<float, weeks::period>;
using days           = std::chrono::days;
using daysf          = duration<float, days::period>;
using hours          = std::chrono::hours;
using hoursf         = duration<float, hours::period>;
using minutes        = std::chrono::minutes;
using minutesf       = duration<float, minutes::period>;
using seconds        = std::chrono::seconds;
using secondsf       = duration<float, seconds::period>;
using milliseconds   = std::chrono::milliseconds;
using millisecondsf  = duration<float, milliseconds::period>;
using microseconds   = std::chrono::microseconds;
using microsecondsf  = duration<float, microseconds::period>;
using nanoseconds    = std::chrono::nanoseconds;
using nanosecondsf   = duration<float, nanoseconds::period>;

template <typename Period>
[[nodiscard]] constexpr std::string_view DurationUnit() {
	if constexpr (std::ratio_equal_v<Period, weeks::period>) {
		return "w";
	} else if constexpr (std::ratio_equal_v<Period, days::period>) {
		return "d";
	} else if constexpr (std::ratio_equal_v<Period, hours::period>) {
		return "h";
	} else if constexpr (std::ratio_equal_v<Period, minutes::period>) {
		return "m";
	} else if constexpr (std::ratio_equal_v<Period, seconds::period>) {
		return "s";
	} else if constexpr (std::ratio_equal_v<Period, milliseconds::period>) {
		return "ms";
	} else if constexpr (std::ratio_equal_v<Period, microseconds::period>) {
		return "us";
	} else if constexpr (std::ratio_equal_v<Period, nanoseconds::period>) {
		return "ns";
	} else {
		return "[custom unit]";
	}
}

template <DurationType T>
[[nodiscard]] constexpr std::string_view DurationUnit() {
	using Period = typename std::remove_cvref_t<T>::period;
	return DurationUnit<Period>();
}

template <typename Rep, typename Period>
std::ostream& operator<<(std::ostream& os, const duration<Rep, Period>& d) {
	return os << d.count() << ' ' << DurationUnit<Period>();
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

		j = std::format("{}{}", d.count(), DurationUnit<Period>());
	}

	static void from_json(const json& j, ptgn::duration<Rep, Period>& d) {
		using namespace ptgn;

		if (!j.is_string()) {
			PTGN_ERROR("Expected duration as string");
		}

		std::string s{ j.get<std::string>() };
		std::smatch match;

		if (std::regex pattern{
				R"(^\s*([+-]?(?:\d+(?:\.\d*)?|\.\d+))\s*(ns|us|ms|m|min|s|h|d|w)\s*$)",
				std::regex::icase
			};
			!std::regex_match(s, match, pattern)) {
			PTGN_ERROR("Invalid duration format: ", s);
		}

		const float value{ std::stof(match[1].str()) };
		std::string unit{ match[2].str() };

		std::ranges::transform(unit, unit.begin(), [](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});

		using Duration = ptgn::duration<Rep, Period>;

		if (unit == "w") {
			d = duration_cast<Duration>(weeksf{ value });
		} else if (unit == "d") {
			d = duration_cast<Duration>(daysf{ value });
		} else if (unit == "h") {
			d = duration_cast<Duration>(hoursf{ value });
		} else if (unit == "m" || unit == "min") {
			d = duration_cast<Duration>(minutesf{ value });
		} else if (unit == "s") {
			d = duration_cast<Duration>(secondsf{ value });
		} else if (unit == "ms") {
			d = duration_cast<Duration>(millisecondsf{ value });
		} else if (unit == "us") {
			d = duration_cast<Duration>(microsecondsf{ value });
		} else if (unit == "ns") {
			d = duration_cast<Duration>(nanosecondsf{ value });
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
