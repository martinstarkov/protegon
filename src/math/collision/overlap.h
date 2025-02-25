#pragma once

#include <vector>

#include "math/geometry/axis.h"
#include "math/vector2.h"

namespace ptgn {

namespace impl {

[[nodiscard]] bool PolygonsHaveOverlapAxis(
	const std::vector<V2_float>& polygonA, const std::vector<V2_float>& polygonB
);

[[nodiscard]] bool PolygonContainsPolygon(
	const std::vector<V2_float>& polygonA, const std::vector<V2_float>& polygonB
);

[[nodiscard]] bool GetPolygonMinimumOverlap(
	const std::vector<V2_float>& polygonA, const std::vector<V2_float>& polygonB, float& depth,
	Axis& axis
);

} // namespace impl

[[nodiscard]] bool OverlapPointLine(
	const V2_float& point, const V2_float& line_start, const V2_float& line_end
);

[[nodiscard]] bool OverlapPointTriangle(
	const V2_float& point, const V2_float& triangle_a, const V2_float& triangle_b,
	const V2_float& triangle_c
);

[[nodiscard]] bool OverlapPointCircle(
	const V2_float& point, const V2_float& circle_center, float circle_radius
);

[[nodiscard]] bool OverlapPointRect(
	const V2_float& point, const V2_float& rect_min, const V2_float& rect_max, float rotation,
	const V2_float& rotation_center
);

[[nodiscard]] bool OverlapPointCapsule(
	const V2_float& point, const V2_float& capsule_start, const V2_float& capsule_end,
	float capsule_radius
);

[[nodiscard]] bool OverlapPointPolygon(const V2_float& point, const std::vector<V2_float>& polygon);

[[nodiscard]] bool OverlapLineLine(
	const V2_float& lineA_start, const V2_float& lineA_end, const V2_float& lineB_start,
	const V2_float& lineB_end
);

[[nodiscard]] bool OverlapLineCircle(
	const V2_float& line_start, const V2_float& line_end, const V2_float& circle_center,
	float circle_radius
);

[[nodiscard]] bool OverlapLineRect(
	const V2_float& line_start, const V2_float& line_end, const V2_float& rect_min,
	const V2_float& rect_max
);

[[nodiscard]] bool OverlapLineCapsule(
	const V2_float& line_start, const V2_float& line_end, const V2_float& capsule_start,
	const V2_float& capsule_end, float capsule_radius
);

[[nodiscard]] bool OverlapCircleCircle(
	const V2_float& circleA_center, float circleA_radius, const V2_float& circleB_center,
	float circleB_radius
);

[[nodiscard]] bool OverlapCircleRect(
	const V2_float& circle_center, float circle_radius, const V2_float& rect_min,
	const V2_float& rect_max
);

[[nodiscard]] bool OverlapCircleCapsule(
	const V2_float& circle_center, float circle_radius, const V2_float& capsule_start,
	const V2_float& capsule_end, float capsule_radius
);

[[nodiscard]] bool OverlapTriangleRect(
	const V2_float& triangle_a, const V2_float& triangle_b, const V2_float& triangle_c,
	const V2_float& rect_min, const V2_float& rect_max, float rotation,
	const V2_float& rotation_center
);

[[nodiscard]] bool OverlapRectRect(
	const V2_float& rectA_min, const V2_float& rectA_max, float rotationA,
	const V2_float& rotation_centerA, const V2_float& rectB_min, const V2_float& rectB_max,
	float rotationB, const V2_float& rotation_centerB
);

[[nodiscard]] bool OverlapRectCapsule(
	const V2_float& rect_min, const V2_float& rect_max, float rotation,
	const V2_float& rotation_center, const V2_float& capsule_start, const V2_float& capsule_end,
	float capsule_radius
);

[[nodiscard]] bool OverlapRectPolygon(
	const V2_float& rect_min, const V2_float& rect_max, float rotation,
	const V2_float& rotation_center, const std::vector<V2_float>& polygon
);

[[nodiscard]] bool OverlapCapsuleCapsule(
	const V2_float& capsuleA_start, const V2_float& capsuleA_end, float capsuleA_radius,
	const V2_float& capsuleB_start, const V2_float& capsuleB_end, float capsuleB_radius
);

[[nodiscard]] bool OverlapPolygonPolygon(
	const std::vector<V2_float>& polygonA, const std::vector<V2_float>& polygonB
);

} // namespace ptgn