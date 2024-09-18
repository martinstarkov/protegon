#pragma once

#include <vector>

#include "protegon/polygon.h"
#include "protegon/vector2.h"
#include "renderer/origin.h"

namespace ptgn {

struct Collider {
	V2_float offset;
	Rectangle<float> bounds;
};

struct BoxCollider : public Collider {
	V2_float size;
	Origin origin{ Origin::Center };
};

struct CircleCollider : public Collider {
	float radius{ 0.0f };
};

// struct PolygonCollider : public Collider {
//	std::vector<V2_float> vertices;
// };

// TODO: Add edge collider.

} // namespace ptgn