#pragma once

#include <cstdint>
#include <ostream>

namespace ptgn {

enum class FontStyle : std::uint32_t {
	Normal		  = 0,
	Bold		  = 1 << 0,
	Italic		  = 1 << 1,
	Underline	  = 1 << 2,
	Strikethrough = 1 << 3,
};

std::ostream& operator<<(std::ostream& os, FontStyle style);

inline FontStyle operator|(FontStyle a, FontStyle b) {
	return static_cast<FontStyle>(std::to_underlying(a) | std::to_underlying(b));
}

inline bool HasFlag(FontStyle value, FontStyle flag) {
	return (std::to_underlying(value) & std::to_underlying(flag)) != 0u;
}

} // namespace ptgn