#pragma once

#include "ecs/components/transform.h"
#include "math/geometry/capsule.h"
#include "math/geometry/circle.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/geometry/rect.h"
#include "math/geometry/shape.h"
#include "math/vector2.h"
#include "serialization/json/serialize.h"

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
	V2_float ray_start, V2_float ray_end, const Transform& transform2, const Line& B
);

[[nodiscard]] RaycastResult RaycastCircle(
	V2_float ray_start, V2_float ray_end, const Transform& transform2, const Circle& B
);

[[nodiscard]] RaycastResult RaycastRect(
	V2_float ray_start, V2_float ray_end, const Transform& transform2, const Rect& B
);

[[nodiscard]] RaycastResult RaycastCapsule(
	V2_float ray_start, V2_float ray_end, const Transform& transform2,
	const Capsule& B
);

[[nodiscard]] RaycastResult RaycastPolygon(
	V2_float ray_start, V2_float ray_end, const Transform& transform2,
	const Polygon& B
);

[[nodiscard]] RaycastResult RaycastCircleLine(
	V2_float ray, const Transform& transform1, const Circle& A, const Transform& transform2,
	const Line& B
);

[[nodiscard]] RaycastResult RaycastCirclePolygon(
	V2_float ray, const Transform& transform1, const Circle& A, const Transform& transform2,
	const Polygon& B
);

[[nodiscard]] RaycastResult RaycastCircleCircle(
	V2_float ray, const Transform& transform1, const Circle& A, const Transform& transform2,
	const Circle& B
);

[[nodiscard]] RaycastResult RaycastCircleRect(
	V2_float ray, const Transform& transform1, const Circle& A, const Transform& transform2,
	const Rect& B
);

[[nodiscard]] RaycastResult RaycastCircleCapsule(
	V2_float ray, const Transform& transform1, const Circle& A, const Transform& transform2,
	const Capsule& B
);

[[nodiscard]] RaycastResult RaycastRectCircle(
	V2_float ray, const Transform& transform1, const Rect& A, const Transform& transform2,
	const Circle& B
);

[[nodiscard]] RaycastResult RaycastRectRect(
	V2_float ray, const Transform& transform1, const Rect& A, const Transform& transform2,
	const Rect& B
);

[[nodiscard]] RaycastResult RaycastRectPolygon(
	V2_float ray, const Transform& transform1, const Rect& A, const Transform& transform2,
	const Polygon& B
);

[[nodiscard]] RaycastResult RaycastPolygonPolygon(
	V2_float ray, const Transform& transform1, const Polygon& A, const Transform& transform2,
	const Polygon& B
);

[[nodiscard]] RaycastResult RaycastCapsuleCircle(
	V2_float ray, const Transform& transform1, const Capsule& A, const Transform& transform2,
	const Circle& B
);

} // namespace impl

[[nodiscard]] RaycastResult Raycast(
	V2_float ray, const Transform& transform1, const ColliderShape& shape1,
	const Transform& transform2, const ColliderShape& shape2
);

} // namespace ptgn