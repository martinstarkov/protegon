#pragma once

#include <utility>
#include <vector>

#include "core/math/geometry/axis.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/geometry/triangle.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"

namespace ptgn {

namespace impl {

std::vector<Axis> GetPolygonAxes(
	const V2_float* vertices, std::size_t vertex_count, bool intersection_info
);

/// @return { min, max } of all the polygon vertices projected onto the given axis.
std::pair<float, float> GetPolygonProjectionMinMax(
	const V2_float* vertices, std::size_t vertex_count, const Axis& axis
);

[[nodiscard]] bool PolygonsHaveOverlapAxis(
	Transform t1, const Polygon& A, Transform t2, const Polygon& B
);

bool GetPolygonMinimumOverlap(
	Transform t1, const Polygon& A, Transform t2, const Polygon& B, float& depth, Axis& axis
);

[[nodiscard]] bool LineContainsLine(Transform t1, const Line& A, Transform t2, const Line& B);

[[nodiscard]] bool PolygonContainsPolygon(
	Transform t1, const Polygon& A, Transform t2, const Polygon& B
);

[[nodiscard]] bool TriangleContainsTriangle(
	Transform t1, const Triangle& A, Transform t2, const Triangle& B
);

[[nodiscard]] bool PolygonContainsTriangle(
	Transform t1, const Polygon& A, Transform t2, const Triangle& B
);

[[nodiscard]] bool OverlapPointPoint(Transform t1, V2_float A, Transform t2, V2_float B);

[[nodiscard]] bool OverlapPointLine(Transform t1, V2_float A, Transform t2, const Line& B);

[[nodiscard]] bool OverlapPointTriangle(Transform t1, V2_float A, Transform t2, const Triangle& B);

[[nodiscard]] bool OverlapPointCircle(Transform t1, V2_float A, Transform t2, const Circle& B);

[[nodiscard]] bool OverlapPointRect(Transform t1, V2_float A, Transform t2, const Rect& B);

[[nodiscard]] bool OverlapPointCapsule(Transform t1, V2_float A, Transform t2, const Capsule& B);

[[nodiscard]] bool OverlapPointPolygon(Transform t1, V2_float A, Transform t2, const Polygon& B);

[[nodiscard]] bool OverlapLineLine(Transform t1, const Line& A, Transform t2, const Line& B);

[[nodiscard]] bool OverlapLineCircle(Transform t1, const Line& A, Transform t2, const Circle& B);

[[nodiscard]] bool OverlapLineTriangle(
	Transform t1, const Line& A, Transform t2, const Triangle& B
);

[[nodiscard]] bool OverlapLineRect(Transform t1, const Line& A, Transform t2, const Rect& B);

[[nodiscard]] bool OverlapLineCapsule(Transform t1, const Line& A, Transform t2, const Capsule& B);

[[nodiscard]] bool OverlapLinePolygon(Transform t1, const Line& A, Transform t2, const Polygon& B);

[[nodiscard]] bool OverlapCircleCircle(
	Transform t1, const Circle& A, Transform t2, const Circle& B
);

[[nodiscard]] bool OverlapCircleTriangle(
	Transform t1, const Circle& A, Transform t2, const Triangle& B
);

[[nodiscard]] bool OverlapCircleRect(Transform t1, const Circle& A, Transform t2, const Rect& B);

[[nodiscard]] bool OverlapCirclePolygon(
	Transform t1, const Circle& A, Transform t2, const Polygon& B
);

[[nodiscard]] bool OverlapCircleCapsule(
	Transform t1, const Circle& A, Transform t2, const Capsule& B
);

[[nodiscard]] bool OverlapTriangleTriangle(
	Transform t1, const Triangle& A, Transform t2, const Triangle& B
);

[[nodiscard]] bool OverlapTriangleRect(
	Transform t1, const Triangle& A, Transform t2, const Rect& B
);

[[nodiscard]] bool OverlapTrianglePolygon(
	Transform t1, const Triangle& A, Transform t2, const Polygon& B
);

[[nodiscard]] bool OverlapTriangleCapsule(
	Transform t1, const Triangle& A, Transform t2, const Capsule& B
);

[[nodiscard]] bool OverlapRectRect(Transform t1, const Rect& A, Transform t2, const Rect& B);

[[nodiscard]] bool OverlapRectCapsule(Transform t1, const Rect& A, Transform t2, const Capsule& B);

[[nodiscard]] bool OverlapRectPolygon(Transform t1, const Rect& A, Transform t2, const Polygon& B);

[[nodiscard]] bool OverlapPolygonPolygon(
	Transform t1, const Polygon& A, Transform t2, const Polygon& B
);

[[nodiscard]] bool OverlapPolygonCapsule(
	Transform t1, const Polygon& A, Transform t2, const Capsule& B
);

[[nodiscard]] bool OverlapCapsuleCapsule(
	Transform t1, const Capsule& A, Transform t2, const Capsule& B
);

} // namespace impl

[[nodiscard]] bool Overlap(
	Transform t1, const ColliderShape& shape1, Transform t2, const ColliderShape& shape2
);

[[nodiscard]] bool Overlap(V2_float point, Transform t2, const ColliderShape& shape2);

[[nodiscard]] bool Overlap(Transform t1, const ColliderShape& shape1, V2_float point);

} // namespace ptgn