#pragma once

#include <cstdint>

#include "math/vector2.h"

namespace ptgn {

struct Color;

struct Axis {
	V2_float direction;
	V2_float midpoint;

	void Draw(const Color& color, float line_width = 1.0f, std::int32_t render_layer = 0) const;
};

} // namespace ptgn