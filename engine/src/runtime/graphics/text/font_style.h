#pragma once

#include <cstdint>
#include <ostream>
#include <utility>

#include "core/log.h"

namespace ptgn {

enum class FontStyle : std::uint32_t {
	Normal		  = 0,
	Bold		  = 1 << 0,
	Italic		  = 1 << 1,
	Underline	  = 1 << 2,
	Strikethrough = 1 << 3,
};

inline FontStyle operator|(FontStyle a, FontStyle b) {
	return static_cast<FontStyle>(std::to_underlying(a) | std::to_underlying(b));
}

inline bool HasFlag(FontStyle value, FontStyle flag) {
	return (std::to_underlying(value) & std::to_underlying(flag)) != 0u;
}

inline std::ostream& operator<<(std::ostream& os, FontStyle style) {
	using enum FontStyle;

	if (style == Normal) {
		return os << "Normal";
	}

	bool first = true;

	auto print = [&](FontStyle flag, const char* name) {
		if (HasFlag(style, flag)) {
			if (!first) {
				os << " | ";
			}
			os << name;
			first = false;
		}
	};

	print(Bold, "Bold");
	print(Italic, "Italic");
	print(Underline, "Underline");
	print(Strikethrough, "Strikethrough");

	if (first) {
		// No known flags matched
		PTGN_ERROR("Unknown FontStyle: ", std::to_underlying(style));
	}

	return os;
}

} // namespace ptgn