#pragma once

#include "ecs/ecs.h"
#include "protegon/vector2.h"

namespace ptgn {

struct Transform {
	ecs::Entity entity;
	V2_float position;
	float rotation{ 0.0f };
	V2_float scale{ 1.0f, 1.0f };
};

} // namespace ptgn