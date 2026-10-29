#pragma once

#include "runtime/ecs/entity.h"
#include "runtime/graphics/fx/effect_registration.h"

namespace ptgn {

class DrawContext;

struct Sharpen {
	static void Draw(DrawContext& ctx, Entity entity);
};

PTGN_REGISTER_EFFECT(Sharpen);

} // namespace ptgn