#pragma once

#include "math/vector2.h"
#include "serialization/json/serializable.h"

namespace ptgn {

struct Axis {
	V2_float direction;
	V2_float midpoint;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Axis, direction, midpoint)
};

} // namespace ptgn