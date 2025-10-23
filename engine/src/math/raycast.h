#pragma once

#include "core/ecs/components/transform.h"
#include "math/geometry/capsule.h"
#include "math/geometry/circle.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/geometry/rect.h"
#include "math/geometry/shape.h"
#include "math/vector2.h"
#include "serialization/json/serializable.h"

namespace ptgn {

struct RaycastResult {
	float t{ 1.0f }; // How far along the ray the impact occurred.
	V2_float normal; // Normal of the impact (normalised).

	operator bool() const {
		return Occurred();
	};

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(RaycastResult, t, normal)

	[[nodiscard]] bool Occurred() const;
};

namespace impl {

[[nodiscard]] RaycastResult RaycastLine(
	const V2_float& ray_start, const V2_float& ray_end, const Transform& transform2, const Line& B
);

[[nodiscard]] RaycastResult RaycastCircle(
	const V2_float& ray_start, const V2_float& ray_end, const Transform& transform2, const Circle& B
);

[[nodiscard]] RaycastResult RaycastRect(
	const V2_float& ray_start, const V2_float& ray_end, const Transform& transform2, const Rect& B
);

[[nodiscard]] RaycastResult RaycastCapsule(
	const V2_float& ray_start, const V2_float& ray_end, const Transform& transform2,
	const Capsule& B
);

[[nodiscard]] RaycastResult RaycastPolygon(
	const V2_float& ray_start, const V2_float& ray_end, const Transform& transform2,
	const Polygon& B
);

[[nodiscard]] RaycastResult RaycastCircleLine(
	const V2_float& ray, const Transform& transform1, const Circle& A, const Transform& transform2,
	const Line& B
);

[[nodiscard]] RaycastResult RaycastCirclePolygon(
	const V2_float& ray, const Transform& transform1, const Circle& A, const Transform& transform2,
	const Polygon& B
);

[[nodiscard]] RaycastResult RaycastCircleCircle(
	const V2_float& ray, const Transform& transform1, const Circle& A, const Transform& transform2,
	const Circle& B
);

[[nodiscard]] RaycastResult RaycastCircleRect(
	const V2_float& ray, const Transform& transform1, const Circle& A, const Transform& transform2,
	const Rect& B
);

[[nodiscard]] RaycastResult RaycastCircleCapsule(
	const V2_float& ray, const Transform& transform1, const Circle& A, const Transform& transform2,
	const Capsule& B
);

[[nodiscard]] RaycastResult RaycastRectCircle(
	const V2_float& ray, const Transform& transform1, const Rect& A, const Transform& transform2,
	const Circle& B
);

[[nodiscard]] RaycastResult RaycastRectRect(
	const V2_float& ray, const Transform& transform1, const Rect& A, const Transform& transform2,
	const Rect& B
);

[[nodiscard]] RaycastResult RaycastRectPolygon(
	const V2_float& ray, const Transform& transform1, const Rect& A, const Transform& transform2,
	const Polygon& B
);

[[nodiscard]] RaycastResult RaycastPolygonPolygon(
	const V2_float& ray, const Transform& transform1, const Polygon& A, const Transform& transform2,
	const Polygon& B
);

[[nodiscard]] RaycastResult RaycastCapsuleCircle(
	const V2_float& ray, const Transform& transform1, const Capsule& A, const Transform& transform2,
	const Circle& B
);

} // namespace impl

[[nodiscard]] RaycastResult Raycast(
	const V2_float& ray, const Transform& transform1, const ColliderShape& shape1,
	const Transform& transform2, const ColliderShape& shape2
);

} // namespace ptgn