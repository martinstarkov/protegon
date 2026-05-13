#pragma once

#include "runtime/ecs/entity.h"
#include "runtime/graphics/fx/effect_registry.h"

namespace ptgn {

class RenderGraphBuilder;

struct BloomEffect {
	float threshold{ 0.8f };
	float radius{ 8.0f };
	int iterations{ 4 };
	float intensity{ 1.0f };

	static void Apply(RenderGraphBuilder& g, Entity effect);
};

PTGN_REGISTER_EFFECT("bloom", BloomEffect);

} // namespace ptgn