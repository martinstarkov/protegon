#pragma once

#include "math/vector2.h"

namespace ptgn {

struct LayerInfo;
struct Color;

struct Axis {
	V2_float direction;
	V2_float midpoint;

	// Uses default render target.
	void Draw(const Color& color, float line_width = 1.0f) const;
	
	void Draw(const Color& color, float line_width, const LayerInfo& layer_info) const;
};

} // namespace ptgn