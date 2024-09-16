#pragma once

#include <vector>

#include "ecs/ecs.h"
#include "protegon/polygon.h"
#include "protegon/vector2.h"

namespace ptgn {

struct Collider {
	ecs::Entity entity;
	V2_float offset;
	Rectangle<float> bounds;
};

struct BoxCollider : public Collider {
	V2_float size;
};

struct CircleCollider : public Collider {
	float radius{ 0.0f };
};

struct PolygonCollider : public Collider {
	std::vector<V2_float> vertices;
};

// TODO: Add edge collider.

} // namespace ptgn