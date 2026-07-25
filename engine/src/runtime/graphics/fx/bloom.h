#pragma once

#include "core/graphics/color.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/fx/effect_registration.h"
#include "serialization/serialize.h"

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

	PTGN_REFLECT(Bloom, threshold, soft_knee, radius, intensity, blur_iterations, tint)
};

PTGN_REGISTER_EFFECT(Bloom, { .hdr = true });

} // namespace ptgn