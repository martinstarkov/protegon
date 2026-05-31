#pragma once

#include "core/graphics/color.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/drawable.h"

namespace ptgn {

class DrawContext;

struct GaussianBlur {
	float radius{ 1.0f };
	std::size_t iterations{ 4 };

	static void Draw(DrawContext& ctx, Entity entity);
};

PTGN_REGISTER_DRAWABLE(GaussianBlur);

} // namespace ptgn