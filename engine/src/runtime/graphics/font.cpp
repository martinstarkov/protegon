#include "runtime/graphics/font.h"

#include <ostream>
#include <utility>

#include "core/log.h"

namespace ptgn {

std::ostream& operator<<(std::ostream& os, FontRenderMode mode) {
	switch (mode) {
		using enum FontRenderMode;
		case Solid:	  return os << "Solid";
		case Shaded:  return os << "Shaded";
		case Blended: return os << "Blended";
		default:	  PTGN_ERROR("Unknown FontRenderMode: ", std::to_underlying(mode));
	}
}

std::ostream& operator<<(std::ostream& os, FontStyle style) {
	using enum FontStyle;

	if (style == Normal) {
		return os << "Normal";
	}

	bool first = true;

	auto print = [&](FontStyle flag, const char* name) {
		if ((style & flag) == flag) {
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
