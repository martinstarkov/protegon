#pragma once

#include "math/vector2.h"
#include "serialization/serializable.h"

namespace ptgn {

struct RaycastResult {
	float t{ 1.0f }; // How far along the ray the impact occurred.
	V2_float normal; // Normal of the impact (normalised).

	PTGN_SERIALIZER_REGISTER(RaycastResult, t, normal)

	[[nodiscard]] bool Occurred() const;
};

/*

[[nodiscard]] RaycastResult Raycast(const Transform& a, Line A, const Transform& b, Line B);

[[nodiscard]] RaycastResult Raycast(const Transform& a, Line A, const Transform& b, Circle B);

[[nodiscard]] RaycastResult Raycast(const Transform& a, Line A, Transform b, Rect B);

[[nodiscard]] RaycastResult Raycast(const Transform& a, Line A, const Transform& b, Capsule B);

[[nodiscard]] RaycastResult Raycast(
	const Transform& a, Circle A, const V2_float& ray, const Transform& b, Line B
);

[[nodiscard]] RaycastResult Raycast(
	const Transform& a, Circle A, const V2_float& ray, const Transform& b, Circle B
);

[[nodiscard]] RaycastResult Raycast(
	const Transform& a, Circle A, const V2_float& ray, Transform b, Rect B
);

[[nodiscard]] RaycastResult Raycast(
	const Transform& a, Circle A, const V2_float& ray, const Transform& b, Capsule B
);

[[nodiscard]] RaycastResult Raycast(
	Transform a, Rect A, const V2_float& ray, const Transform& b, Circle B
);

[[nodiscard]] RaycastResult Raycast(Transform a, Rect A, const V2_float& ray, Transform b, Rect B);

[[nodiscard]] RaycastResult Raycast(
	const Transform& a, Capsule A, const V2_float& ray, const Transform& b, Circle B
);

*/

namespace impl {

[[nodiscard]] RaycastResult RaycastLineLine(
	const V2_float& lineA_start, const V2_float& lineA_end, const V2_float& lineB_start,
	const V2_float& lineB_end
);

[[nodiscard]] RaycastResult RaycastLineCircle(
	const V2_float& line_start, const V2_float& line_end, const V2_float& circle_center,
	float circle_radius
);

[[nodiscard]] RaycastResult RaycastLineRect(
	const V2_float& line_start, const V2_float& line_end, const V2_float& rect_center,
	const V2_float& rect_size
);

[[nodiscard]] RaycastResult RaycastLineCapsule(
	const V2_float& line_start, const V2_float& line_end, const V2_float& capsule_start,
	const V2_float& capsule_end, float capsule_radius
);

[[nodiscard]] RaycastResult RaycastCircleLine(
	const V2_float& circle_center, float circle_radius, const V2_float& ray,
	const V2_float& line_start, const V2_float& line_end
);

[[nodiscard]] RaycastResult RaycastCircleCircle(
	const V2_float& circleA_center, float circleA_radius, const V2_float& ray,
	const V2_float& circleB_center, float circleB_radius
);

[[nodiscard]] RaycastResult RaycastCircleRect(
	const V2_float& circle_center, float circle_radius, const V2_float& ray,
	const V2_float& rect_center, const V2_float& rect_size
);

[[nodiscard]] RaycastResult RaycastCircleCapsule(
	const V2_float& circle_center, float circle_radius, const V2_float& ray,
	const V2_float& capsule_start, const V2_float& capsule_end, float capsule_radius
);

[[nodiscard]] RaycastResult RaycastRectCircle(
	const V2_float& rect_center, const V2_float& rect_size, const V2_float& ray,
	const V2_float& circle_center, float circle_radius
);

[[nodiscard]] RaycastResult RaycastRectRect(
	const V2_float& rectA_center, const V2_float& rectA_size, const V2_float& ray,
	const V2_float& rectB_center, const V2_float& rectB_size
);

[[nodiscard]] RaycastResult RaycastCapsuleCircle(
	const V2_float& capsule_start, const V2_float& capsule_end, float capsule_radius,
	const V2_float& ray, const V2_float& circle_center, float circle_radius
);

} // namespace impl

} // namespace ptgn