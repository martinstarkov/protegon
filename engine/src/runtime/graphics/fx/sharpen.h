#pragma once

#include "runtime/ecs/entity.h"
#include "runtime/graphics/drawable.h"

namespace ptgn {

class DrawContext;

struct Sharpen {
	static void Draw(DrawContext& ctx, Entity entity);
};

PTGN_REGISTER_DRAWABLE(Sharpen);

} // namespace ptgn