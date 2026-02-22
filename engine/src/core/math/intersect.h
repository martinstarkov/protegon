#pragma once

#include "core/math/geometry/circle.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "serialization/json/serialize.h"

namespace ptgn {

struct Intersection {
	float depth{ 0.0f };
	V2_float normal;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Intersection, depth, normal)

	[[nodiscard]] bool Occurred() const;
};

namespace impl {

[[nodiscard]] Intersection IntersectCircleCircle(
	Transform t1, const Circle& A, Transform t2, const Circle& B
);

[[nodiscard]] Intersection IntersectCircleRect(
	Transform t1, const Circle& A, Transform t2, const Rect& B
);

[[nodiscard]] Intersection IntersectCirclePolygon(
	Transform t1, const Circle& A, Transform t2, const Polygon& B
);

[[nodiscard]] Intersection IntersectRectRect(
	Transform t1, const Rect& A, Transform t2, const Rect& B
);

[[nodiscard]] Intersection IntersectPolygonPolygon(
	Transform t1, const Polygon& A, Transform t2, const Polygon& B
);

} // namespace impl

[[nodiscard]] Intersection Intersect(
	Transform t1, const ColliderShape& shape1, Transform t2, const ColliderShape& shape2
);

} // namespace ptgn