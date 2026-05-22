#pragma once

#include "runtime/ecs/entity.h"
#include "runtime/graphics/drawable.h"

namespace ptgn {

class DrawContext;

struct InverseColor {
	static void Draw(DrawContext& ctx, Entity entity);
};

PTGN_REGISTER_DRAWABLE(InverseColor);

} // namespace ptgn