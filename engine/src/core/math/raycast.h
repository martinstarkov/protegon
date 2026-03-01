#pragma once

#include <ostream>

#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
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

std::ostream& operator<<(std::ostream& os, const RaycastResult& result);

namespace impl {

[[nodiscard]] RaycastResult RaycastLine(
	V2_float ray_start, V2_float ray_end, Transform transform2, const Line& B
);

[[nodiscard]] RaycastResult RaycastCircle(
	V2_float ray_start, V2_float ray_end, Transform transform2, const Circle& B
);

[[nodiscard]] RaycastResult RaycastRect(
	V2_float ray_start, V2_float ray_end, Transform transform2, const Rect& B
);

[[nodiscard]] RaycastResult RaycastCapsule(
	V2_float ray_start, V2_float ray_end, Transform transform2, const Capsule& B
);

[[nodiscard]] RaycastResult RaycastPolygon(
	V2_float ray_start, V2_float ray_end, Transform transform2, const Polygon& B
);

[[nodiscard]] RaycastResult RaycastCircleLine(
	V2_float ray, Transform transform1, const Circle& A, Transform transform2, const Line& B
);

[[nodiscard]] RaycastResult RaycastCirclePolygon(
	V2_float ray, Transform transform1, const Circle& A, Transform transform2, const Polygon& B
);

[[nodiscard]] RaycastResult RaycastCircleCircle(
	V2_float ray, Transform transform1, const Circle& A, Transform transform2, const Circle& B
);

[[nodiscard]] RaycastResult RaycastCircleRect(
	V2_float ray, Transform transform1, const Circle& A, Transform transform2, const Rect& B
);

[[nodiscard]] RaycastResult RaycastCircleCapsule(
	V2_float ray, Transform transform1, const Circle& A, Transform transform2, const Capsule& B
);

[[nodiscard]] RaycastResult RaycastRectCircle(
	V2_float ray, Transform transform1, const Rect& A, Transform transform2, const Circle& B
);

[[nodiscard]] RaycastResult RaycastRectRect(
	V2_float ray, Transform transform1, const Rect& A, Transform transform2, const Rect& B
);

[[nodiscard]] RaycastResult RaycastRectPolygon(
	V2_float ray, Transform transform1, const Rect& A, Transform transform2, const Polygon& B
);

[[nodiscard]] RaycastResult RaycastPolygonPolygon(
	V2_float ray, Transform transform1, const Polygon& A, Transform transform2, const Polygon& B
);

[[nodiscard]] RaycastResult RaycastCapsuleCircle(
	V2_float ray, Transform transform1, const Capsule& A, Transform transform2, const Circle& B
);

} // namespace impl

[[nodiscard]] RaycastResult Raycast(
	V2_float ray, Transform transform1, const ColliderShape& shape1, Transform transform2,
	const ColliderShape& shape2
);

} // namespace ptgn