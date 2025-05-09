#pragma once

#include "math/vector2.h"
#include "serialization/serializable.h"

namespace ptgn {

struct Intersection {
	float depth{ 0.0f };
	V2_float normal;

	PTGN_SERIALIZER_REGISTER(Intersection, depth, normal)

	[[nodiscard]] bool Occurred() const;
};

/*
[[nodiscard]] Intersection Intersects(const Transform& a, Circle A, const Transform& b, Circle B);

[[nodiscard]] Intersection Intersects(const Transform& a, Circle A, Transform b, Rect B);

[[nodiscard]] Intersection Intersects(
	const Transform& a, const Rect& A, const Transform& b, const Circle& B
);

[[nodiscard]] Intersection Intersects(Transform a, Rect A, Transform b, Rect B);

[[nodiscard]] Intersection Intersects(const Transform& a, Polygon A, const Transform& b, Polygon B);
*/

namespace impl {

[[nodiscard]] Intersection IntersectCircleCircle(
	const V2_float& circleA_center, float circleA_radius, const V2_float& circleB_center,
	float circleB_radius
);

[[nodiscard]] Intersection IntersectCircleRect(
	const V2_float& circle_center, float circle_radius, const V2_float& rect_center,
	const V2_float& rect_size
);

[[nodiscard]] Intersection IntersectRectRect(
	const V2_float& rectA_center, const V2_float& rectA_size, float rectA_rotation,
	const V2_float& rectB_center, const V2_float& rectB_size, float rectB_rotation
);

[[nodiscard]] Intersection IntersectPolygonPolygon(
	const V2_float* polygonA_vertices, std::size_t polygonA_vertex_count,
	const V2_float* polygonB_vertices, std::size_t polygonB_vertex_count
);

} // namespace impl

} // namespace ptgn