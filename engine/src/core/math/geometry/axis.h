#pragma once

#include "core/math/vector2.h"
#include "serialization/serialize.h"

namespace ptgn {

struct Axis {
	V2_float direction;
	V2_float midpoint;

	PTGN_REFLECT(Axis, direction, midpoint)
};

} // namespace ptgn