#pragma once

#include <iosfwd>
#include <ostream>

#include "debug/log.h"
#include "math/vector2.h"
#include "serialization/enum.h"

namespace ptgn {

enum class Origin {
	Center,
	TopLeft,
	CenterTop,
	TopRight,
	CenterRight,
	BottomRight,
	CenterBottom,
	BottomLeft,
	CenterLeft,
};

namespace impl {

[[nodiscard]] V2_float GetOriginOffsetHalf(Origin origin, const V2_float& half);

} // namespace impl

// @return Vector to be added to a position to get the object center given an origin and size.
[[nodiscard]] V2_float GetOriginOffset(Origin origin, const V2_float& size);

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

PTGN_SERIALIZER_REGISTER_ENUM(
	Origin, { { Origin::Center, "center" },
			  { Origin::TopLeft, "top_left" },
			  { Origin::CenterTop, "center_top" },
			  { Origin::TopRight, "top_right" },
			  { Origin::CenterRight, "center_right" },
			  { Origin::BottomRight, "bottom_right" },
			  { Origin::CenterBottom, "center_bottom" },
			  { Origin::BottomLeft, "bottom_left" },
			  { Origin::CenterLeft, "center_left" } }
);

} // namespace ptgn