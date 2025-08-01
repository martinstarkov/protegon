#pragma once

#include "components/transform.h"
#include "geometry/capsule.h"
#include "geometry/circle.h"
#include "geometry/line.h"
#include "geometry/polygon.h"
#include "geometry/rect.h"
#include "geometry/triangle.h"
#include "math/geometry.h"
#include "math/geometry/axis.h"

namespace ptgn {

namespace impl {

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
	const Transform& t1, const Point& A, const Transform& t2, const Point& B
);

[[nodiscard]] bool OverlapPointLine(
	const Transform& t1, const Point& A, const Transform& t2, const Line& B
);

[[nodiscard]] bool OverlapPointTriangle(
	const Transform& t1, const Point& A, const Transform& t2, const Triangle& B
);

[[nodiscard]] bool OverlapPointCircle(
	const Transform& t1, const Point& A, const Transform& t2, const Circle& B
);

[[nodiscard]] bool OverlapPointRect(
	const Transform& t1, const Point& A, const Transform& t2, const Rect& B
);

[[nodiscard]] bool OverlapPointCapsule(
	const Transform& t1, const Point& A, const Transform& t2, const Capsule& B
);

[[nodiscard]] bool OverlapPointPolygon(
	const Transform& t1, const Point& A, const Transform& t2, const Polygon& B
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
	const Transform& t1, const Shape& shape1, const Transform& t2, const Shape& shape2
);

[[nodiscard]] bool Overlap(const Point& point, const Transform& t2, const Shape& shape2);

[[nodiscard]] bool Overlap(const Transform& t1, const Shape& shape1, const Point& point);

} // namespace ptgn