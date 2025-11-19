#pragma once

#include "ecs/components/transform.h"
#include "geometry/circle.h"
#include "geometry/polygon.h"
#include "geometry/rect.h"
#include "math/geometry/shape.h"
#include "math/vector2.h"
#include "serialization/json/serializable.h"

namespace ptgn {

struct Intersection {
	float depth{ 0.0f };
	V2_float normal;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Intersection, depth, normal)

	[[nodiscard]] bool Occurred() const;
};

namespace impl {

[[nodiscard]] Intersection IntersectCircleCircle(
	const Transform& t1, const Circle& A, const Transform& t2, const Circle& B
);

[[nodiscard]] Intersection IntersectCircleRect(
	const Transform& t1, const Circle& A, const Transform& t2, const Rect& B
);

[[nodiscard]] Intersection IntersectCirclePolygon(
	const Transform& t1, const Circle& A, const Transform& t2, const Polygon& B
);

[[nodiscard]] Intersection IntersectRectRect(
	const Transform& t1, const Rect& A, const Transform& t2, const Rect& B
);

[[nodiscard]] Intersection IntersectPolygonPolygon(
	const Transform& t1, const Polygon& A, const Transform& t2, const Polygon& B
);

} // namespace impl

[[nodiscard]] Intersection Intersect(
	const Transform& t1, const ColliderShape& shape1, const Transform& t2,
	const ColliderShape& shape2
);

} // namespace ptgn