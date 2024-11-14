#include "math/vector2.h"

#include "core/game.h"
#include "renderer/color.h"
#include "renderer/layer_info.h"
#include "renderer/renderer.h"

namespace ptgn {

namespace impl {

void DrawPoint(float x, float y, const Color& color, float radius, const LayerInfo& layer_info) {
	game.draw.Point({ x, y }, color, radius, layer_info);
}

} // namespace impl

} // namespace ptgn
