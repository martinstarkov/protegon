#pragma once

#include "core/graphics/color.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/drawable.h"

namespace ptgn {

class DrawContext;

struct Glow {
	float threshold{ 0.35f };
	float soft_knee{ 0.35f };
	float radius{ 1.5f };
	float intensity{ 1.25f };
	std::size_t blur_iterations{ 4 };
	Color tint{ color::White };

	static void Draw(DrawContext& ctx, Entity entity);
};

PTGN_REGISTER_DRAWABLE(Glow);

} // namespace ptgn