#pragma once

#include "runtime/ecs/entity.h"
#include "runtime/graphics/fx/effects.h"

namespace ptgn {

class DrawContext;

struct EdgeDetection {
	static void Draw(DrawContext& ctx, Entity entity);
};

PTGN_REGISTER_EFFECT(EdgeDetection, { .name = "Edge Detection" });

} // namespace ptgn