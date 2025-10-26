#pragma once

#include <iomanip>
#include <iosfwd>
#include <sstream>
#include <string>
#include <string_view>

#include "core/util/concepts.h"

// TODO: Get rid of stuff that is outdated as of my C++ 20 move.

namespace ptgn {

template <StreamWritable T>
std::string ToString(const T& object) {
	std::ostringstream ss;
	ss << object;
	return ss.str();
}

// @param precision The number of decimal places of precision to have in numbers converted to
// string.
template <StreamWritable T>
std::string ToString(const T& object, int precision) {
	std::ostringstream ss;
	ss << std::fixed << std::setprecision(precision);
	ss << object;
	if constexpr (std::is_floating_point_v<T>) {
		// Catch and remove -0s. As per: https://stackoverflow.com/a/21538723
		const std::string& t{ ss.str() };
		return t.find_first_of("123456789") == t.npos && t[0] == '-' ? t.substr(1) : t;
	} else {
		return ss.str();
	}
}

std::string ToLower(const std::string& str);

std::string ToUpper(const std::string& str);

// @return True if str begins with the specified prefix.
constexpr bool BeginsWith(std::string_view str, std::string_view prefix) {
	return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}

// @return True if str ends with the specified suffix.
constexpr bool EndsWith(std::string_view str, std::string_view suffix) {
	return str.size() >= suffix.size() &&
		   str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

} // namespace ptgn