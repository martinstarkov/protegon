#pragma once

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

namespace impl {

V2_float GetOriginOffsetHalf(Origin origin, V2_float half);

} // namespace impl

/// @return Vector to be added to a position to get the object center given an origin and size.
V2_float GetOriginOffset(Origin origin, V2_float size);

} // namespace ptgn