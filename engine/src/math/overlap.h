#pragma once

#include "core/ecs/components/transform.h"
#include "math/geometry/axis.h"
#include "math/geometry/capsule.h"
#include "math/geometry/circle.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/geometry/rect.h"
#include "math/geometry/shape.h"
#include "math/geometry/triangle.h"
#include "math/vector2.h"

namespace ptgn {

namespace impl {

[[nodiscard]] std::vector<Axis> GetPolygonAxes(
	const V2_float* vertices, std::size_t vertex_count, bool intersection_info
);

// @return { min, max } of all the polygon vertices projected onto the given axis.
[[nodiscard]] std::pair<float, float> GetPolygonProjectionMinMax(
	const V2_float* vertices, std::size_t vertex_count, const Axis& axis
);

[[nodiscard]] bool PolygonsHaveOverlapAxis(
	const Transform& t1, const Polygon& A, const Transform& t2, const Polygon& B
);

[[nodiscard]] bool GetPolygonMinimumOverlap(
	const Transform& t1, const Polygon& A, const Transform& t2, const Polygon& B, float& depth,
	Axis& axis
);

[[nodiscard]] bool LineContainsLine(
	const Transform& t1, const Line& A, const Transform& t2, const Line& B
);

[[nodiscard]] bool PolygonContainsPolygon(
	const Transform& t1, const Polygon& A, const Transform& t2, const Polygon& B
);

[[nodiscard]] bool TriangleContainsTriangle(
	const Transform& t1, const Triangle& A, const Transform& t2, const Triangle& B
);

[[nodiscard]] bool PolygonContainsTriangle(
	const Transform& t1, const Polygon& A, const Transform& t2, const Triangle& B
);

[[nodiscard]] bool OverlapPointPoint(
	const Transform& t1, const V2_float& A, const Transform& t2, const V2_float& B
);

[[nodiscard]] bool OverlapPointLine(
	const Transform& t1, const V2_float& A, const Transform& t2, const Line& B
);

[[nodiscard]] bool OverlapPointTriangle(
	const Transform& t1, const V2_float& A, const Transform& t2, const Triangle& B
);

[[nodiscard]] bool OverlapPointCircle(
	const Transform& t1, const V2_float& A, const Transform& t2, const Circle& B
);

[[nodiscard]] bool OverlapPointRect(
	const Transform& t1, const V2_float& A, const Transform& t2, const Rect& B
);

[[nodiscard]] bool OverlapPointCapsule(
	const Transform& t1, const V2_float& A, const Transform& t2, const Capsule& B
);

[[nodiscard]] bool OverlapPointPolygon(
	const Transform& t1, const V2_float& A, const Transform& t2, const Polygon& B
);

[[nodiscard]] bool OverlapLineLine(
	const Transform& t1, const Line& A, const Transform& t2, const Line& B
);

[[nodiscard]] bool OverlapLineCircle(
	const Transform& t1, const Line& A, const Transform& t2, const Circle& B
);

[[nodiscard]] bool OverlapLineTriangle(
	const Transform& t1, const Line& A, const Transform& t2, const Triangle& B
);

[[nodiscard]] bool OverlapLineRect(
	const Transform& t1, const Line& A, const Transform& t2, const Rect& B
);

[[nodiscard]] bool OverlapLineCapsule(
	const Transform& t1, const Line& A, const Transform& t2, const Capsule& B
);

[[nodiscard]] bool OverlapLinePolygon(
	const Transform& t1, const Line& A, const Transform& t2, const Polygon& B
);

[[nodiscard]] bool OverlapCircleCircle(
	const Transform& t1, const Circle& A, const Transform& t2, const Circle& B
);

[[nodiscard]] bool OverlapCircleTriangle(
	const Transform& t1, const Circle& A, const Transform& t2, const Triangle& B
);

[[nodiscard]] bool OverlapCircleRect(
	const Transform& t1, const Circle& A, const Transform& t2, const Rect& B
);

[[nodiscard]] bool OverlapCirclePolygon(
	const Transform& t1, const Circle& A, const Transform& t2, const Polygon& B
);

[[nodiscard]] bool OverlapCircleCapsule(
	const Transform& t1, const Circle& A, const Transform& t2, const Capsule& B
);

[[nodiscard]] bool OverlapTriangleTriangle(
	const Transform& t1, const Triangle& A, const Transform& t2, const Triangle& B
);

[[nodiscard]] bool OverlapTriangleRect(
	const Transform& t1, const Triangle& A, const Transform& t2, const Rect& B
);

[[nodiscard]] bool OverlapTrianglePolygon(
	const Transform& t1, const Triangle& A, const Transform& t2, const Polygon& B
);

[[nodiscard]] bool OverlapTriangleCapsule(
	const Transform& t1, const Triangle& A, const Transform& t2, const Capsule& B
);

[[nodiscard]] bool OverlapRectRect(
	const Transform& t1, const Rect& A, const Transform& t2, const Rect& B
);

[[nodiscard]] bool OverlapRectCapsule(
	const Transform& t1, const Rect& A, const Transform& t2, const Capsule& B
);

[[nodiscard]] bool OverlapRectPolygon(
	const Transform& t1, const Rect& A, const Transform& t2, const Polygon& B
);

[[nodiscard]] bool OverlapPolygonPolygon(
	const Transform& t1, const Polygon& A, const Transform& t2, const Polygon& B
);

[[nodiscard]] bool OverlapPolygonCapsule(
	const Transform& t1, const Polygon& A, const Transform& t2, const Capsule& B
);

[[nodiscard]] bool OverlapCapsuleCapsule(
	const Transform& t1, const Capsule& A, const Transform& t2, const Capsule& B
);

} // namespace impl

[[nodiscard]] bool Overlap(
	const Transform& t1, const ColliderShape& shape1, const Transform& t2,
	const ColliderShape& shape2
);

[[nodiscard]] bool Overlap(const V2_float& point, const Transform& t2, const ColliderShape& shape2);

[[nodiscard]] bool Overlap(const Transform& t1, const ColliderShape& shape1, const V2_float& point);

} // namespace ptgn