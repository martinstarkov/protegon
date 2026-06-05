#pragma once

#include "runtime/ecs/entity.h"
#include "runtime/graphics/drawable.h"

namespace ptgn {

class DrawContext;

struct Blur {
	std::size_t iterations{ 4 };

	static void Draw(DrawContext& ctx, Entity entity);
};

PTGN_REGISTER_EFFECT(Blur);

} // namespace ptgn