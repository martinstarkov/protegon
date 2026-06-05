#pragma once

#include "core/graphics/color.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/drawable.h"

namespace ptgn {

class DrawContext;

struct Bloom {
	float threshold{ 0.75f };
	float soft_knee{ 0.20f };
	float radius{ 1.0f };
	float intensity{ 1.0f };
	std::size_t blur_iterations{ 4 };
	Color tint{ color::White };

	static void Draw(DrawContext& ctx, Entity entity);
};

PTGN_REGISTER_EFFECT(Bloom, true);

} // namespace ptgn