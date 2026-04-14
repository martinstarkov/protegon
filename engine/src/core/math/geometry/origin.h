#pragma once

#include <ostream>
#include <utility>

#include "core/log.h"
#include "core/math/vector2.h"
#include "serialization/serialize.h"

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

V2_float GetOriginOffsetHalf(Origin origin, V2_float half);

} // namespace impl

/// @return Vector to be added to a position to get the object center given an origin and size.
V2_float GetOriginOffset(Origin origin, V2_float size);

inline std::ostream& operator<<(std::ostream& os, Origin origin) {
	switch (origin) {
		using enum Origin;
		case TopLeft:	   return os << "TopLeft";
		case CenterTop:	   return os << "CenterTop";
		case TopRight:	   return os << "TopRight";
		case CenterLeft:   return os << "CenterLeft";
		case Center:	   return os << "Center";
		case CenterRight:  return os << "CenterRight";
		case BottomLeft:   return os << "BottomLeft";
		case CenterBottom: return os << "CenterBottom";
		case BottomRight:  return os << "BottomRight";
		default:		   PTGN_ERROR("Unknown Origin: ", std::to_underlying(origin));
	}
}

PTGN_SERIALIZE_ENUM(
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