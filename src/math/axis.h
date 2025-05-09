#pragma once

#include "math/vector2.h"
#include "serialization/serializable.h"

namespace ptgn {

struct Axis {
	V2_float direction;
	V2_float midpoint;

	PTGN_SERIALIZER_REGISTER(Axis, direction, midpoint)
};

} // namespace ptgn