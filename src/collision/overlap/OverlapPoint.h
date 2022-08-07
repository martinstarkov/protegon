#pragma once

#include "math/Vector2.h"
#include "math/Math.h"
#include "utility/TypeTraits.h"
#include "collision/Types.h"

#include "collision/overlap/OverlapAABB.h"
#include "collision/overlap/OverlapCircle.h"

namespace ptgn {

namespace overlap {

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Modified page 79 with size of other AABB set to 0.
// Check if a point an AABB overlap.
// AABB position is taken from top left.
// AABB size is the full extent from top left to bottom right.
template <typename T>
inline bool PointAABB(const Point<T>& p,
					  const AABB<T>& a) {
	return AABBAABB({ p, { T{ 0 }, T{ 0 } } }, a);
}

// Check if a point and a capsule overlap.
// Capsule origin and destination are taken from the edge circle centers.
template <typename T>
inline bool PointCapsule(const Point<T>& p,
						   const Capsule<T>& a) {
	return CircleCapsule({ p, T{ 0 } }, a);
}

// Source: https://www.jeffreythompson.org/collision-detection/point-circle.php
// Source (used): https://doubleroot.in/lessons/circle/position-of-a-point/#:~:text=If%20the%20distance%20is%20greater,As%20simple%20as%20that!
// Check if a point and a circle overlap.
// Circle position is taken from its center.
template <typename T>
inline bool PointCircle(const Point<T>& p,
						const Circle<T>& a) {
	return CircleCircle({ p, T{ 0 } }, a);
}

// Source: https://www.jeffreythompson.org/collision-detection/line-point.php
// Source (used): https://stackoverflow.com/a/7050238
template <typename T, typename S = double,
	tt::floating_point<S> = true>
inline bool PointLine(const Point<T>& p,
					  const Line<T>& a) {
	const math::Vector2<S> ap{ p.p - a.origin };
	const math::Vector2<S> dir{ a.Direction() };
	const math::Vector2<S> grad{ ap / dir };
	// Check that the gradient is the same along both axes, i.e. "colinear".
	const math::Vector2<T> min{ math::Min(a.origin, a.destination) };
	const math::Vector2<T> max{ math::Max(a.origin, a.destination) };
	// Edge cases where line aligns with an axis.
	// TODO: Check that this is correct.
	if (math::Compare(dir.x, 0) && math::Compare(p.p.x, a.origin.x)) {
		if (p.p.y < min.y || p.p.y > max.y) return false;
		return true;
	}
	if (math::Compare(dir.y, 0) && math::Compare(p.p.y, a.origin.y)) {
		if (p.p.x < min.x || p.p.x > max.x) return false;
		return true;
	}
	return grad.IsEqual() && PointAABB(p, { min, max - min });
}

template <typename T>
inline bool PointPoint(const Point<T>& p,
					   const Point<T>& o_p) {
	return p.p == o_p.p;
}

} // namespace overlap

} // namespace ptgn