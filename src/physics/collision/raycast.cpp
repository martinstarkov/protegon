#include "physics/collision/raycast.h"

#include <cmath>
#include <utility>

#include "common/assert.h"
#include "core/game.h"
#include "debug/debug.h"
#include "debug/stats.h"
#include "math/math.h"
#include "math/vector2.h"
#include "physics/collision/overlap.h"

namespace ptgn {

bool RaycastResult::Occurred() const {
	PTGN_ASSERT(t >= 0.0f);
	return t >= 0.0f && t < 1.0f && !normal.IsZero();
}

namespace impl {

RaycastResult RaycastLineLine(
	const V2_float& lineA_start, const V2_float& lineA_end, const V2_float& lineB_start,
	const V2_float& lineB_end
) {
#ifdef PTGN_DEBUG
	game.stats.raycast_line_line++;
#endif
	// Source:
	// https://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect/565282#565282

	RaycastResult c;

	// TODO: Move to using a general overlap check.
	if (!OverlapLineLine(lineA_start, lineA_end, lineB_start, lineB_end)) {
		return c;
	}

	V2_float r{ lineA_end - lineA_start };
	V2_float s{ lineB_end - lineB_start };

	float sr{ s.Cross(r) };
	if (NearlyEqual(sr, 0.0f)) {
		return c;
	}

	V2_float ab{ lineA_start - lineB_start };
	float abr{ ab.Cross(r) };

	if (float u{ abr / sr }; u < 0.0f || u > 1.0f) {
		return c;
	}

	V2_float ba{ -ab };
	float rs{ r.Cross(s) };
	if (NearlyEqual(rs, 0.0f)) {
		return c;
	}

	V2_float skewed{ -s.Skewed() };
	float mag2{ skewed.Dot(skewed) };
	if (NearlyEqual(mag2, 0.0f)) {
		return c;
	}

	float bas{ ba.Cross(s) };
	float t{ bas / rs };

	if (t < 0.0f || t >= 1.0f) {
		return c;
	}

	c.t		 = t;
	c.normal = skewed / std::sqrt(mag2);

	return c;
}

RaycastResult RaycastLineCircle(
	const V2_float& line_start, const V2_float& line_end, const V2_float& circle_center,
	float circle_radius
) {
#ifdef PTGN_DEBUG
	game.stats.raycast_line_circle++;
#endif
	// Source:
	// https://stackoverflow.com/questions/1073336/circle-line-segment-collision-detection-algorithm/1084899#1084899

	RaycastResult c;

	if (!OverlapLineCircle(line_start, line_end, circle_center, circle_radius)) {
		return c;
	}

	V2_float d{ -(line_end - line_start) };
	V2_float f{ circle_center - line_start };

	// bool (roots exist), float (root 1), float (root 2).
	auto [real, t1, t2] =
		QuadraticFormula(d.Dot(d), 2.0f * f.Dot(d), f.Dot(f) - circle_radius * circle_radius);

	if (!real) {
		return c;
	}

	bool w1{ t1 >= 0.0f && t1 < 1.0f };
	bool w2{ t2 >= 0.0f && t2 < 1.0f };

	// Pick the lowest collision time that is in the [0, 1] range.
	if (w1 && w2) {
		c.t = std::min(t1, t2);
	} else if (w1) {
		c.t = t1;
	} else if (w2) {
		c.t = t2;
	} else {
		return c;
	}

	V2_float impact{ circle_center + d * c.t - line_start };

	float mag2{ impact.Dot(impact) };

	// TODO: Sometimes when mag2 is nearly equal to circle.radius^2 a swept circle sliding along the
	// top of a rectangle will stick to the line vertices. However adding the NearlyEqual check for
	// this condition leads to bugs with raycasting a line through a circle.
	if (NearlyEqual(mag2, 0.0f) /* || NearlyEqual(mag2, circle.radius * circle.radius)*/) {
		c = {};
		return c;
	}

	c.normal = -impact / std::sqrt(mag2);

	return c;
}

RaycastResult RaycastLineRect(
	const V2_float& line_start, const V2_float& line_end, const V2_float& rect_center,
	const V2_float& rect_size
) {
#ifdef PTGN_DEBUG
	game.stats.raycast_line_rect++;
#endif
	RaycastResult c;

	bool start_in{ OverlapPointRect(line_start, rect_center, rect_size, 0.0f) };

	bool end_in{ OverlapPointRect(line_end, rect_center, rect_size, 0.0f) };

	if (start_in && end_in) {
		return c;
	}

	V2_float d{ line_end - line_start };

	if (d.Dot(d) == 0.0f) {
		return c;
	}

	V2_float inv_dir{ 1.0f / d };

	auto half{ rect_size * 0.5f };
	auto rect_min{ rect_center - half };
	auto rect_max{ rect_center + half };

	// Calculate intersections with rectangle bounding axes.
	V2_float near{ rect_min - line_start };
	V2_float far{ rect_max - line_start };

	// Handle edge cases where the segment line is parallel with the edge of the rectangle.
	if (NearlyEqual(near.x, 0.0f)) {
		near.x = 0.0f;
	}
	if (NearlyEqual(near.y, 0.0f)) {
		near.y = 0.0f;
	}
	if (NearlyEqual(far.x, 0.0f)) {
		far.x = 0.0f;
	}
	if (NearlyEqual(far.y, 0.0f)) {
		far.y = 0.0f;
	}

	V2_float t_near{ near * inv_dir };
	V2_float t_far{ far * inv_dir };

	// Discard 0 / 0 divisions.
	if (std::isnan(t_far.y) || std::isnan(t_far.x)) {
		return c;
	}
	if (std::isnan(t_near.y) || std::isnan(t_near.x)) {
		return c;
	}

	// Sort axis collision times so t_near contains the shorter time.
	if (t_near.x > t_far.x) {
		std::swap(t_near.x, t_far.x);
	}
	if (t_near.y > t_far.y) {
		std::swap(t_near.y, t_far.y);
	}

	// Early rejection.
	if (t_near.x >= t_far.y || t_near.y >= t_far.x) {
		return c;
	}

	// Furthest time is contact on opposite side of target.
	// Reject if furthest time is negative, meaning the object is travelling away from the
	// target.
	float t_hit_far{ std::min(t_far.x, t_far.y) };

	if (t_hit_far < 0.0f) {
		return c;
	}

	if (NearlyEqual(t_near.x, t_near.y) && t_near.x == 1.0f) {
		return c;
	}

	// Closest time will be the first contact.
	bool interal{ start_in && !end_in };

	float time{ 1.0f };

	if (interal) {
		std::swap(t_near.x, t_far.x);
		std::swap(t_near.y, t_far.y);
		std::swap(inv_dir.x, inv_dir.y);
		time  = std::min(t_near.x, t_near.y);
		d	 *= -1.0f;
	} else {
		time = std::max(t_near.x, t_near.y);
	}

	if (time < 0.0f || time >= 1.0f) {
		return c;
	}

	c.t = time;

	// Contact point of collision from parametric line equation.
	// c.point = a.a + c.time * d;

	// Find which axis collides further along the movement time.

	// TODO: Figure out how to fix biasing of one direction from one side and another on the
	// other side.
	bool equal_times{ NearlyEqual(t_near.x, t_near.y) };
	bool diagonal{ NearlyEqual(Abs(inv_dir.x), Abs(inv_dir.y)) };

	if (equal_times && diagonal) { // Both axes collide at the same time.
		// Diagonal collision, set normal to opposite of direction of movement.
		c.normal = { -Sign(d.x), -Sign(d.y) };
	}
	if (c.normal.IsZero()) {
		if (t_near.x > t_near.y) { // X-axis.
			// Direction of movement.
			if (inv_dir.x < 0.0f) {
				c.normal = { 1.0f, 0.0f };
			} else {
				c.normal = { -1.0f, 0.0f };
			}
		} else if (t_near.x < t_near.y) { // Y-axis.
			// Direction of movement.
			if (inv_dir.y < 0.0f) {
				c.normal = { 0.0f, 1.0f };
			} else {
				c.normal = { 0.0f, -1.0f };
			}
		}
	}

	if (interal) {
		std::swap(c.normal.x, c.normal.y);
		c.normal *= -1.0f;
	}

	// RaycastResult collision occurred.
	return c;
}

RaycastResult RaycastLineCapsule(
	const V2_float& line_start, const V2_float& line_end, const V2_float& capsule_start,
	const V2_float& capsule_end, float capsule_radius
) {
#ifdef PTGN_DEBUG
	game.stats.raycast_line_capsule++;
#endif
	// Source: https://stackoverflow.com/a/52462458

	RaycastResult c;

	// TODO: Add early exit if overlap test fails.

	V2_float cv{ line_end - line_start };
	float mag2{ cv.Dot(cv) };

	if (NearlyEqual(mag2, 0.0f)) {
		return RaycastLineCircle(line_start, line_end, capsule_start, capsule_radius);
	}

	float mag{ std::sqrt(mag2) };
	V2_float cu{ cv / mag };
	// Normal to b.line
	V2_float ncu{ cu.Skewed() };
	V2_float ncu_dist{ ncu * capsule_radius };

	RaycastResult col_min{ c };

	auto c1{ RaycastLineLine(line_start, line_end, line_start + ncu_dist, line_end + ncu_dist) };
	auto c2{ RaycastLineLine(line_start, line_end, line_start - ncu_dist, line_end - ncu_dist) };
	auto c3{ RaycastLineCircle(line_start, line_end, capsule_start, capsule_radius) };
	auto c4{ RaycastLineCircle(line_start, line_end, capsule_end, capsule_radius) };

	if (c1.Occurred() && c1.t < col_min.t) {
		col_min = c1;
	}
	if (c2.Occurred() && c2.t < col_min.t) {
		col_min = c2;
	}
	if (c3.Occurred() && c3.t < col_min.t) {
		col_min = c3;
	}
	if (c4.Occurred() && c4.t < col_min.t) {
		col_min = c4;
	}

	if (NearlyEqual(col_min.t, 1.0f)) {
		c = {};
		return c;
	}

	c = col_min;

	return c;
}

RaycastResult RaycastCircleLine(
	const V2_float& circle_center, float circle_radius, const V2_float& ray,
	const V2_float& line_start, const V2_float& line_end
) {
	return RaycastLineCapsule(
		circle_center, circle_center + ray, line_start, line_end, circle_radius
	);
}

RaycastResult RaycastCircleCircle(
	const V2_float& circleA_center, float circleA_radius, const V2_float& ray,
	const V2_float& circleB_center, float circleB_radius
) {
	return RaycastLineCircle(
		circleA_center, circleA_center + ray, circleB_center, circleA_radius + circleB_radius
	);
}

RaycastResult RaycastCircleRect(
	const V2_float& circle_center, float circle_radius, const V2_float& ray,
	const V2_float& rect_center, const V2_float& rect_size
) {
#ifdef PTGN_DEBUG
	game.stats.raycast_circle_rect++;
#endif
	// TODO: Fix corner collisions.
	// TODO: Consider
	// https://www.geometrictools.com/Documentation/IntersectionMovingCircleRectangle.pdf
	/*return Rect{ center, { 2.0f * radius, 2.0f * radius }, Origin::Center, 0.0f }.RaycastResult(
		ray, rect
	);*/
	/*V2_float rect_min{ rect.Min() };
	V2_float rect_max{ rect.Max() };
	auto r1 = RaycastResult(ray, Circle{ rect_min, radius });
	auto r2 = RaycastResult(ray, Circle{ V2_float{ rect_max.x, rect_min.y }, radius });
	auto r3 = RaycastResult(ray, Circle{ rect_max, radius });
	auto r4 = RaycastResult(ray, Circle{ V2_float{ rect_min.x, rect_max.y }, radius });*/

	RaycastResult c;

	V2_float ray_end{ circle_center + ray };

	/*bool start_inside{ Overlaps(rect) };
	bool end_inside{ rect.Overlaps(Circle{ seg.b, radius }) };*/

	// if (start_inside) {
	//	// Circle inside rectangle, flip segment direction.
	//	std::swap(seg.a, seg.b);
	// }

	/*if (!OverlapLineRect(circle_center, ray_end, rect_center, rect_size + circle_radius * 2.0f)) {
	return c;
	}*/

	RaycastResult col_min{ c };

	auto half{ rect_size * 0.5f };
	auto rect_min{ rect_center - half };
	auto rect_max{ rect_center + half };

	// Top segment.
	auto c1{ RaycastLineCapsule(
		circle_center, ray_end, rect_min, V2_float{ rect_max.x, rect_min.y }, circle_radius
	) };

	// Right segment.
	auto c2{ RaycastLineCapsule(
		circle_center, ray_end, V2_float{ rect_max.x, rect_min.y }, rect_max, circle_radius
	) };

	// Bottom segment.
	auto c3{ RaycastLineCapsule(
		circle_center, ray_end, rect_max, V2_float{ rect_min.x, rect_max.y }, circle_radius
	) };

	// Left segment.
	auto c4{ RaycastLineCapsule(
		circle_center, ray_end, V2_float{ rect_min.x, rect_max.y }, rect_min, circle_radius
	) };

	if (c1.Occurred() && c1.t < col_min.t) {
		col_min = c1;
	}
	if (c2.Occurred() && c2.t < col_min.t) {
		col_min = c2;
	}
	if (c3.Occurred() && c3.t < col_min.t) {
		col_min = c3;
	}
	if (c4.Occurred() && c4.t < col_min.t) {
		col_min = c4;
	}

	if (col_min.t < 0.0f || col_min.t >= 1.0f) {
		return c;
	}

	// if (start_inside) {
	//	col_min.t = 1.0f - col_min.t;
	// }

	c = col_min;

	return c;
}

RaycastResult RaycastCircleCapsule(
	const V2_float& circle_center, float circle_radius, const V2_float& ray,
	const V2_float& capsule_start, const V2_float& capsule_end, float capsule_radius
) {
	return RaycastLineCapsule(
		circle_center, circle_center + ray, capsule_start, capsule_end,
		circle_radius + capsule_radius
	);
}

RaycastResult RaycastRectCircle(
	const V2_float& rect_center, const V2_float& rect_size, const V2_float& ray,
	const V2_float& circle_center, float circle_radius
) {
	return RaycastCircleRect(circle_center, circle_radius, -ray, rect_center, rect_size);
}

RaycastResult RaycastRectRect(
	const V2_float& rectA_center, const V2_float& rectA_size, const V2_float& ray,
	const V2_float& rectB_center, const V2_float& rectB_size
) {
#ifdef PTGN_DEBUG
	game.stats.raycast_rect_rect++;
#endif
	return RaycastLineRect(rectA_center, rectA_center + ray, rectB_center, rectA_size + rectB_size);
}

RaycastResult RaycastCapsuleCircle(
	const V2_float& capsule_start, const V2_float& capsule_end, float capsule_radius,
	const V2_float& ray, const V2_float& circle_center, float circle_radius
) {
	return RaycastCircleCapsule(
		circle_center, circle_radius, -ray, capsule_start, capsule_end, capsule_radius
	);
}

} // namespace impl

/*

// TODO: Readd somehow otherwise.
RaycastResult Raycast(const Transform& a, Line A, const Transform& b, Line B) {
	A.start *= a.scale;
	A.end	*= a.scale;
	A.start += a.position;
	A.end	+= a.position;
	B.start *= b.scale;
	B.end	*= b.scale;
	B.start += b.position;
	B.end	+= b.position;
	return impl::RaycastLineLine(A.start, A.end, B.start, B.end);
}

RaycastResult Raycast(const Transform& a, Line A, const Transform& b, Circle B) {
	A.start	 *= a.scale;
	A.end	 *= a.scale;
	A.start	 += a.position;
	A.end	 += a.position;
	B.radius *= b.scale.x;
	return impl::RaycastLineCircle(A.start, A.end, b.position, B.radius);
}

RaycastResult Raycast(const Transform& a, Line A, Transform b, Rect B) {
	A.start	   *= a.scale;
	A.end	   *= a.scale;
	A.start	   += a.position;
	A.end	   += a.position;
	B.size	   *= b.scale;
	b.position += GetOriginOffset(B.origin, B.size);
	return impl::RaycastLineRect(A.start, A.end, b.position, B.size);
}

RaycastResult Raycast(const Transform& a, Line A, const Transform& b, Capsule B) {
	A.start	 *= a.scale;
	A.end	 *= a.scale;
	A.start	 += a.position;
	A.end	 += a.position;
	B.start	 *= b.scale;
	B.end	 *= b.scale;
	B.start	 += b.position;
	B.end	 += b.position;
	B.radius *= b.scale.x;
	return impl::RaycastLineCapsule(A.start, A.end, B.start, B.end, B.radius);
}

RaycastResult Raycast(
	const Transform& a, Circle A, const V2_float& ray, const Transform& b, Line B
) {
	A.radius *= a.scale.x;
	B.start	 *= b.scale;
	B.end	 *= b.scale;
	B.start	 += b.position;
	B.end	 += b.position;
	return impl::RaycastCircleLine(a.position, A.radius, ray, B.start, B.end);
}

RaycastResult Raycast(
	const Transform& a, Circle A, const V2_float& ray, const Transform& b, Circle B
) {
	A.radius *= a.scale.x;
	B.radius *= b.scale.x;
	return impl::RaycastCircleCircle(a.position, A.radius, ray, b.position, B.radius);
}

RaycastResult Raycast(const Transform& a, Circle A, const V2_float& ray, Transform b, Rect B) {
	A.radius   *= a.scale.x;
	B.size	   *= b.scale;
	b.position += GetOriginOffset(B.origin, B.size);
	return impl::RaycastCircleRect(a.position, A.radius, ray, b.position, B.size);
}

RaycastResult Raycast(
	const Transform& a, Circle A, const V2_float& ray, const Transform& b, Capsule B
) {
	A.radius *= a.scale.x;
	B.start	 *= b.scale;
	B.end	 *= b.scale;
	B.start	 += b.position;
	B.end	 += b.position;
	B.radius *= b.scale.x;
	return impl::RaycastCircleCapsule(a.position, A.radius, ray, B.start, B.end, B.radius);
}

RaycastResult Raycast(Transform a, Rect A, const V2_float& ray, const Transform& b, Circle B) {
	A.size	   *= a.scale;
	a.position += GetOriginOffset(A.origin, A.size);
	B.radius   *= b.scale.x;
	return impl::RaycastRectCircle(a.position, A.size, ray, b.position, B.radius);
}

RaycastResult Raycast(Transform a, Rect A, const V2_float& ray, Transform b, Rect B) {
	A.size	   *= a.scale;
	a.position += GetOriginOffset(A.origin, A.size);
	B.size	   *= b.scale;
	b.position += GetOriginOffset(B.origin, B.size);
	return impl::RaycastRectRect(a.position, A.size, ray, b.position, B.size);
}

RaycastResult Raycast(
	const Transform& a, Capsule A, const V2_float& ray, const Transform& b, Circle B
) {
	A.start	 *= a.scale;
	A.end	 *= a.scale;
	A.start	 += a.position;
	A.end	 += a.position;
	A.radius *= a.scale.x;
	B.radius *= b.scale.x;
	return impl::RaycastCapsuleCircle(A.start, A.end, A.radius, ray, b.position, B.radius);
}

*/

} // namespace ptgn