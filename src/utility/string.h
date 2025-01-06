#pragma once

#include <iomanip>
#include <iosfwd>
#include <sstream>
#include <string>

#include "utility/type_traits.h"

namespace ptgn {

template <typename T, tt::stream_writable<std::ostream, T> = true>
std::string ToString(const T& object) {
	std::ostringstream ss;
	ss << object;
	return ss.str();
}

// @param precision The number of decimal places of precision to have in numbers converted to
// string.
template <typename T, tt::stream_writable<std::ostream, T> = true>
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

} // namespace ptgn