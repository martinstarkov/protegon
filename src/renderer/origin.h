#pragma once

#include <iosfwd>
#include <ostream>

#include "math/vector2.h"
#include "utility/log.h"

namespace ptgn {

enum class Origin {
	TopLeft,
	CenterTop,
	TopRight,
	CenterLeft,
	Center,
	CenterRight,
	BottomLeft,
	CenterBottom,
	BottomRight,
};

// @return If draw_origin is Origin::TopLeft, returned offset will be 0, if draw_origin is
// Origin::Center, returned offset will be size / 2, etc.
[[nodiscard]] V2_float GetOffsetFromCenter(const V2_float& size, Origin draw_origin);

inline std::ostream& operator<<(std::ostream& os, Origin origin) {
	switch (origin) {
		case Origin::TopLeft:	   os << "Top Left"; break;
		case Origin::CenterTop:	   os << "Center Top"; break;
		case Origin::TopRight:	   os << "Top Right"; break;
		case Origin::CenterLeft:   os << "Center Left"; break;
		case Origin::Center:	   os << "Center"; break;
		case Origin::CenterRight:  os << "Center Right"; break;
		case Origin::BottomLeft:   os << "Bottom Left"; break;
		case Origin::CenterBottom: os << "Center Bottom"; break;
		case Origin::BottomRight:  os << "Bottom Right"; break;
		default:				   PTGN_ERROR("Invalid origin");
	}

	return os;
}

} // namespace ptgn