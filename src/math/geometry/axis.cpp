#include "math/geometry/axis.h"

#include "core/game.h"
#include "core/window.h"
#include "math/geometry/line.h"
#include "renderer/color.h"
#include "renderer/layer_info.h"

namespace ptgn {

void Axis::Draw(const Color& color, float line_width) const {
	Draw(color, line_width, {});
}

void Axis::Draw(const Color& color, float line_width, const LayerInfo& layer_info) const {
	V2_float ws{ game.window.GetSize() };
	float mag{ ws.MagnitudeSquared() };
	// Find line points on the window extents.
	V2_float p0{ midpoint + direction * mag };
	V2_float p1{ midpoint - direction * mag };

	Line l{ p0, p1 };
	l.Draw(color, line_width, layer_info);
}

} // namespace ptgn