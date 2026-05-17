#pragma once

#include "runtime/ecs/entity.h"

namespace ptgn {

class DrawContext;

struct GlowEffect {
	static void Draw(DrawContext& ctx, Entity effect);
};

} // namespace ptgn