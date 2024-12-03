#include "math/geometry/axis.h"

#include "core/game.h"
#include "renderer/color.h"
#include "renderer/renderer.h"

namespace ptgn {

void Axis::Draw(const Color& color, float line_width, const LayerInfo& layer_info) const {
	game.draw.Axis(midpoint, direction, color, line_width, layer_info);
}

} // namespace ptgn