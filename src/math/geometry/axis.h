#pragma once

#include "math/vector2.h"
#include "renderer/layer_info.h"

namespace ptgn {

struct Color;

struct Axis {
	V2_float direction;
	V2_float midpoint;

	void Draw(const Color& color, float line_width = 1.0f, const LayerInfo& layer_info = {}) const;
};

} // namespace ptgn