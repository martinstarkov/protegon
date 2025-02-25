#include "math/collision/intersect.h"

#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "core/game.h"
#include "math/collision/overlap.h"
#include "math/geometry/axis.h"
#include "math/geometry/polygon.h"
#include "math/math.h"
#include "math/utility.h"
#include "math/vector2.h"
#include "utility/assert.h"
#include "utility/debug.h"
#include "utility/utility.h"

namespace ptgn {

bool Intersection::Occurred() const {
	PTGN_ASSERT(
		depth >= 0.0f && depth != std::numeric_limits<float>::infinity(),
		"Failed to identify correct intersection depth"
	);
	return !normal.IsZero();
}

Intersection IntersectCircleCircle(
	const V2_float& circleA_center, float circleA_radius, const V2_float& circleB_center,
	float circleB_radius
) {
	Intersection c;

	V2_float d{ circleB_center - circleA_center };
	float dist2{ d.Dot(d) };
	float r{ circleA_radius + circleB_radius };

	// No overlap.
	if (!impl::WithinPerimeter(r, dist2)) {
#ifdef PTGN_DEBUG
		game.stats.overlap_circle_circle++;
#endif
		return c;
	}

#ifdef PTGN_DEBUG
	game.stats.intersect_circle_circle++;
#endif

	if (dist2 > epsilon2<float>) {
		float dist{ std::sqrt(dist2) };
		PTGN_ASSERT(!NearlyEqual(dist, 0.0f));
		c.normal = -d / dist;
		c.depth	 = r - dist;
	} else {
		// Edge case where circle centers are in the same location.
		c.normal.y = -1.0f; // default to upward normal.
		c.depth	   = r;
	}

	c.depth = std::max(c.depth, 0.0f);

	return c;
}

Intersection IntersectCircleRect(
	const V2_float& circle_center, float circle_radius, const V2_float& rect_min,
	const V2_float& rect_max
) {
#ifdef PTGN_DEBUG
	game.stats.intersect_circle_rect++;
#endif
	// Source:
	// https://steamcdn-a.akamaihd.net/apps/valve/2015/DirkGregorius_Contacts.pdf
	Intersection c;

	V2_float half{ (rect_max - rect_min) / 2.0f };
	V2_float max{ rect_max };
	V2_float min{ rect_min };
	V2_float clamped{ Clamp(circle_center, min, max) };
	V2_float ab{ circle_center - clamped };

	float dist2{ ab.Dot(ab) };

	// No overlap.
	if (!impl::WithinPerimeter(circle_radius, dist2)) {
		return c;
	}

	if (!NearlyEqual(dist2, 0.0f)) {
		// Shallow intersection (center of circle not inside of AABB).
		float d{ std::sqrt(dist2) };
		PTGN_ASSERT(!NearlyEqual(d, 0.0f));
		c.normal = ab / d;
		c.depth	 = circle_radius - d;
		c.depth	 = std::max(c.depth, 0.0f);
		return c;
	}

	// Deep intersection (center of circle inside of AABB).

	// Clamp circle's center to edge of AABB, then form the manifold.
	V2_float mid{ Midpoint(rect_min, rect_max) };
	V2_float d{ mid - circle_center };

	if (V2_float overlap{ half - V2_float{ FastAbs(d.x), FastAbs(d.y) } }; overlap.x < overlap.y) {
		c.depth	   = circle_radius + overlap.x;
		c.normal.x = d.x < 0 ? 1.0f : -1.0f;
	} else {
		c.depth	   = circle_radius + overlap.y;
		c.normal.y = d.y < 0 ? 1.0f : -1.0f;
	}

	PTGN_ASSERT(c.depth >= 0.0f);

	return c;
}

Intersection IntersectRectCircle(
	const V2_float& rect_min, const V2_float& rect_max, const V2_float& circle_center,
	float circle_radius
) {
	Intersection c{ IntersectCircleRect(circle_center, circle_radius, rect_min, rect_max) };
	c.normal *= -1.0f;
	return c;
}

Intersection IntersectRectRect(
	const V2_float& rectA_min, const V2_float& rectA_max, float rotationA,
	const V2_float& rotation_centerA, const V2_float& rectB_min, const V2_float& rectB_max,
	float rotationB, const V2_float& rotation_centerB
) {
	Intersection c;

	if (rotationA != 0.0f || rotationB != 0.0f) {
		auto verticesA{ impl::GetQuadVertices(rectA_min, rectA_max, rotationA, rotation_centerA) };
		auto verticesB{ impl::GetQuadVertices(rectB_min, rectB_max, rotationB, rotation_centerB) };
		return IntersectPolygonPolygon(ToVector(verticesA), ToVector(verticesB));
	}

#ifdef PTGN_DEBUG
	game.stats.intersect_rect_rect++;
#endif

	V2_float a_h{ (rectA_max - rectA_min) * 0.5f };
	V2_float b_h{ (rectB_max - rectB_min) * 0.5f };
	V2_float d{ Midpoint(rectB_min, rectB_max) - Midpoint(rectA_min, rectA_max) };
	V2_float pen{ a_h + b_h - V2_float{ FastAbs(d.x), FastAbs(d.y) } };

	// Optional: To include seams in collision, simply remove the NearlyEqual calls from this if
	// statement.
	if (pen.x < 0 || pen.y < 0 || NearlyEqual(pen.x, 0.0f) || NearlyEqual(pen.y, 0.0f)) {
		return c;
	}

	if (NearlyEqual(d.x, 0.0f) && NearlyEqual(d.y, 0.0f)) {
		// Edge case where aabb centers are in the same location.
		c.normal.y = -1.0f; // upward
		c.depth	   = a_h.y + b_h.y;
	} else if (pen.y < pen.x) {
		c.normal.y = -Sign(d.y);
		c.depth	   = FastAbs(pen.y);
	} else {
		c.normal.x = -Sign(d.x);
		c.depth	   = FastAbs(pen.x);
	}

	PTGN_ASSERT(c.depth >= 0.0f);

	return c;
}

Intersection IntersectPolygonPolygon(
	const std::vector<V2_float>& pA, const std::vector<V2_float>& pB
) {
#ifdef PTGN_DEBUG
	game.stats.intersect_polygon_polygon++;
#endif
	PTGN_ASSERT(
		impl::IsConvexPolygon(pA) && impl::IsConvexPolygon(pB),
		"PolygonPolygon intersection check only works if both polygons are convex"
	);

	Intersection c;

	float depth{ std::numeric_limits<float>::infinity() };
	Axis axis;

	if (!impl::GetPolygonMinimumOverlap(pA, pB, depth, axis) ||
		!impl::GetPolygonMinimumOverlap(pB, pA, depth, axis)) {
		return c;
	}

	PTGN_ASSERT(depth != std::numeric_limits<float>::infinity());
	PTGN_ASSERT(depth >= 0.0f);

	// Make sure the vector is pointing from polygon1 to polygon2.
	if (V2_float dir{ impl::GetPolygonCenter(pA) - impl::GetPolygonCenter(pB) };
		dir.Dot(axis.direction) < 0) {
		axis.direction *= -1.0f;
	}

	c.normal = axis.direction;
	c.depth	 = depth;

	return c;

	/*
	// Useful debug drawing code:
	// Draw all polygon points projected onto all the axes.
	const auto draw_axes = [](const std::vector<Axis>& axes, const Polygon& p) {
		for (const auto& a : axes) {
			game.draw.Axis(a.midpoint, a.direction, color::Pink, 1.0f);
			auto [min, max] = impl::GetProjectionMinMax(p, a);
			V2_float p1{ a.midpoint + a.direction * min };
			V2_float p2{ a.midpoint + a.direction * max };
			game.draw.Point(p1, color::Purple, 5.0f);
			game.draw.Point(p2, color::Orange, 5.0f);
			V2_float to{ 0.0f, -17.0f };
			game.draw.Text(std::to_string((int)min), p1 + to, color::Purple);
			game.draw.Text(std::to_string((int)max), p2 + to, color::Orange);
		}
	};

	draw_axes(impl::GetAxes(*this, true), *this);
	draw_axes(impl::GetAxes(*this, true), polygon);
	draw_axes(impl::GetAxes(polygon, true), *this);
	draw_axes(impl::GetAxes(polygon, true), polygon);

	// Draw overlap axis and overlap amounts on both sides.
	game.draw.Axis(axis.midpoint, c.normal, color::Black, 2.0f);
	game.draw.Line(axis.midpoint, axis.midpoint - c.normal * c.depth, color::Cyan, 3.0f);
	game.draw.Line(axis.midpoint, axis.midpoint + c.normal * c.depth, color::Cyan, 3.0f);
	*/
}

} // namespace ptgn