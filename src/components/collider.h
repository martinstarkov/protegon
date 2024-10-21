#pragma once

#include <vector>

#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/origin.h"

namespace ptgn {

struct Collider {
	V2_float offset;
	Rect bounds;
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