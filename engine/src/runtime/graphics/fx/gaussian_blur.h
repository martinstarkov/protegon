#pragma once

#include "runtime/ecs/entity.h"
#include "runtime/graphics/fx/effect_registration.h"
#include "serialization/serialize.h"

namespace ptgn {

class DrawContext;

struct GaussianBlur {
	float radius{ 1.0f };
	std::size_t iterations{ 4 };

	static void Draw(DrawContext& ctx, Entity entity);

	PTGN_REFLECT(GaussianBlur, radius, iterations)
};

PTGN_REGISTER_EFFECT(GaussianBlur, { .name = "Gaussian Blur" });

} // namespace ptgn