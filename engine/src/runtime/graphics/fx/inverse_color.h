#pragma once

#include "runtime/ecs/entity.h"
#include "runtime/graphics/fx/effect_registration.h"

namespace ptgn {

class DrawContext;

struct InverseColor {
	static void Draw(DrawContext& ctx, Entity entity);
};

PTGN_REGISTER_EFFECT(InverseColor, { .name = "Inverse Color" });

} // namespace ptgn