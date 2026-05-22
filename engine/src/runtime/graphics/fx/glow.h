#pragma once

#include "runtime/ecs/entity.h"
#include "runtime/graphics/drawable.h"

namespace ptgn {

class DrawContext;

struct Glow {
	static void Draw(DrawContext& ctx, Entity entity);
};

PTGN_REGISTER_DRAWABLE(Glow);

} // namespace ptgn