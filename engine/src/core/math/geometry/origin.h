#pragma once

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
PTGN_SERIALIZE_ENUM(Origin);

inline constexpr Origin kDefaultOrigin{ Origin::Center };

/// @return Vector to be added to a position to get the object center given an origin and size.
constexpr V2_float GetOffset(Origin origin, V2_float size) {
	auto half{ size * 0.5f };

	switch (origin) {
		using enum Origin;
		case Center:	   return {};
		case TopLeft:	   return half;
		case CenterBottom: return V2_float{ 0.0f, -half.y };
		case CenterTop:	   return { 0.0f, half.y };
		case BottomRight:  return -half;
		case BottomLeft:   return V2_float{ half.x, -half.y };
		case TopRight:	   return V2_float{ -half.x, half.y };
		case CenterLeft:   return { half.x, 0.0f };
		case CenterRight:  return { -half.x, 0.0f };
		default:		   PTGN_ERROR("Unknown Origin: ", std::to_underlying(origin));
	}
}

} // namespace ptgn