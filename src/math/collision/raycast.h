#pragma once

#include "math/vector2.h"

namespace ptgn {

struct Raycast {
	float t{ 1.0f }; // How far along the ray the impact occurred.
	V2_float normal; // Normal of the impact (normalised).

	[[nodiscard]] bool Occurred() const;
};

[[nodiscard]] Raycast RaycastLineLine(
	const V2_float& lineA_start, const V2_float& lineA_end, const V2_float& lineB_start,
	const V2_float& lineB_end
);

[[nodiscard]] Raycast RaycastLineCircle(
	const V2_float& line_start, const V2_float& line_end, const V2_float& circle_center,
	float circle_radius
);

[[nodiscard]] Raycast RaycastLineRect(
	const V2_float& line_start, const V2_float& line_end, const V2_float& rect_min,
	const V2_float& rect_max
);

[[nodiscard]] Raycast RaycastLineCapsule(
	const V2_float& line_start, const V2_float& line_end, const V2_float& capsule_start,
	const V2_float& capsule_end, float capsule_radius
);

[[nodiscard]] Raycast RaycastCircleLine(
	const V2_float& circle_center, float circle_radius, const V2_float& ray,
	const V2_float& line_start, const V2_float& line_end
);

[[nodiscard]] Raycast RaycastCircleCircle(
	const V2_float& circleA_center, float circleA_radius, const V2_float& ray,
	const V2_float& circleB_center, float circleB_radius
);

[[nodiscard]] Raycast RaycastCircleRect(
	const V2_float& circle_center, float circle_radius, const V2_float& ray,
	const V2_float& rect_min, const V2_float& rect_max
);

[[nodiscard]] Raycast RaycastCircleCapsule(
	const V2_float& circle_center, float circle_radius, const V2_float& ray,
	const V2_float& capsule_start, const V2_float& capsule_end, float capsule_radius
);

[[nodiscard]] Raycast RaycastRectCircle(
	const V2_float& rect_min, const V2_float& rect_max, const V2_float& ray,
	const V2_float& circle_center, float circle_radius
);

[[nodiscard]] Raycast RaycastRectRect(
	const V2_float& rectA_min, const V2_float& rectA_max, const V2_float& ray,
	const V2_float& rectB_min, const V2_float& rectB_max
);

[[nodiscard]] Raycast RaycastCapsuleCircle(
	const V2_float& capsule_start, const V2_float& capsule_end, float capsule_radius,
	const V2_float& ray, const V2_float& circle_center, float circle_radius
);

} // namespace ptgn