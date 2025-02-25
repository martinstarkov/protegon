#pragma once

#include <vector>

#include "math/vector2.h"

namespace ptgn {

struct Intersection {
	float depth{ 0.0f };
	V2_float normal;

	[[nodiscard]] bool Occurred() const;
};

[[nodiscard]] Intersection IntersectCircleCircle(
	const V2_float& circleA_center, float circleA_radius, const V2_float& circleB_center,
	float circleB_radius
);

[[nodiscard]] Intersection IntersectCircleRect(
	const V2_float& circle_center, float circle_radius, const V2_float& rect_min,
	const V2_float& rect_max
);

[[nodiscard]] Intersection IntersectRectCircle(
	const V2_float& rect_min, const V2_float& rect_max, const V2_float& circle_center,
	float circle_radius
);

[[nodiscard]] Intersection IntersectRectRect(
	const V2_float& rectA_min, const V2_float& rectA_max, float rotationA,
	const V2_float& rotation_centerA, const V2_float& rectB_min, const V2_float& rectB_max,
	float rotationB, const V2_float& rotation_centerB
);

[[nodiscard]] Intersection IntersectPolygonPolygon(
	const std::vector<V2_float>& polygonA, const std::vector<V2_float>& polygonB
);

} // namespace ptgn