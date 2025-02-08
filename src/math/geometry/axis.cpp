#include "math/geometry/axis.h"

#include <cstdint>

#include "core/game.h"
#include "core/window.h"
#include "math/geometry/line.h"
#include "renderer/color.h"

namespace ptgn {

// TODO: Get rid of this.
void Axis::Draw(const Color& color, float line_width, std::int32_t render_layer) const {
	V2_float ws{ game.window.GetSize() };
	float mag{ ws.MagnitudeSquared() };
	// Find line points on the window extents.
	V2_float p0{ midpoint + direction * mag };
	V2_float p1{ midpoint - direction * mag };

	Line l{ p0, p1 };
	// TODO: Fix.
	// l.Draw(color, line_width, render_layer);
}

} // namespace ptgn