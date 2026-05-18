#pragma once

#include "runtime/ecs/entity.h"

namespace ptgn {

struct BloomEffect {
	float threshold{ 0.8f };
	float radius{ 8.0f };
	int iterations{ 4 };
	float intensity{ 1.0f };
};

} // namespace ptgn