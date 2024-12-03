#pragma once

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

} // namespace ptgn