#include "physics/collision/intersect.h"

#include <cmath>
#include <limits>
#include <utility>

#include "common/assert.h"
#include "core/game.h"
#include "debug/debugging.h"
#include "math/axis.h"
#include "math/geometry.h"
#include "math/math.h"
#include "math/utility.h"
#include "math/vector2.h"
#include "physics/collision/overlap.h"
#include "rendering/api/origin.h"

namespace ptgn {

bool Intersection::Occurred() const {
	PTGN_ASSERT(
		depth >= 0.0f && depth != std::numeric_limits<float>::infinity(),
		"Failed to identify correct intersection depth"
	);
	return !normal.IsZero();
}

namespace impl {

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
	const V2_float& circle_center, float circle_radius, const V2_float& rect_center,
	const V2_float& rect_size, float rect_rotation, std::optional<V2_float> rect_rotation_center
) {
	if (rect_rotation != 0.0f) {
		auto rect_polygon{ GetVertices(
			{ rect_center, rect_rotation }, rect_size, Origin::Center, rect_rotation_center
		) };
		return IntersectCirclePolygon(
			circle_center, circle_radius, rect_polygon.data(), rect_polygon.size()
		);
	}

#ifdef PTGN_DEBUG
	game.stats.intersect_circle_rect++;
#endif
	// Source:
	// https://steamcdn-a.akamaihd.net/apps/valve/2015/DirkGregorius_Contacts.pdf
	Intersection c;

	V2_float half{ rect_size * 0.5f };
	V2_float clamped{ Clamp(circle_center, rect_center - half, rect_center + half) };
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
	V2_float mid{ rect_center };
	V2_float d{ mid - circle_center };

	if (V2_float overlap{ half - V2_float{ Abs(d.x), Abs(d.y) } }; overlap.x < overlap.y) {
		c.depth	   = circle_radius + overlap.x;
		c.normal.x = d.x < 0 ? 1.0f : -1.0f;
	} else {
		c.depth	   = circle_radius + overlap.y;
		c.normal.y = d.y < 0 ? 1.0f : -1.0f;
	}

	PTGN_ASSERT(c.depth >= 0.0f);

	return c;
}

Intersection IntersectCirclePolygon(
	const V2_float& circle_center, float circle_radius, const V2_float* polygon_vertices,
	std::size_t polygon_vertex_count
) {
	Intersection c;
#ifdef PTGN_DEBUG
	game.stats.intersect_circle_polygon++;
#endif

	float min_penetration{ std::numeric_limits<float>::infinity() };
	V2_float collision_normal;
	bool intersection_occurred{ false };

	// Check each edge of the polygon
	for (std::size_t i{ 0 }; i < polygon_vertex_count; ++i) {
		const V2_float& a{ polygon_vertices[i] };
		const V2_float& b{ polygon_vertices[(i + 1) % polygon_vertex_count] };
		V2_float edge{ b - a };
		V2_float edge_normal{ edge.Skewed().Normalized() }; // outward normal

		// Project circle center onto edge normal
		float distance_to_edge{ edge_normal.Dot(circle_center - a) };

		if (distance_to_edge > circle_radius) {
			// No intersection — circle is outside
			return c; // c.Occurred() == false
		}

		// Track the deepest penetration
		float penetration{ circle_radius - distance_to_edge };
		if (penetration < min_penetration) {
			min_penetration	 = penetration;
			collision_normal = edge_normal;
		}
	}

	// If we got here, the circle intersects or is inside the polygon
	PTGN_ASSERT(min_penetration != std::numeric_limits<float>::infinity());
	PTGN_ASSERT(!collision_normal.IsZero());
	c.depth	 = min_penetration;
	c.normal = collision_normal;
	return c;
}

Intersection IntersectRectRect(
	const V2_float& rectA_center, const V2_float& rectA_size, float rectA_rotation,
	std::optional<V2_float> rectA_rotation_center, const V2_float& rectB_center,
	const V2_float& rectB_size, float rectB_rotation, std::optional<V2_float> rectB_rotation_center
) {
	Intersection c;

	if (rectA_rotation != 0.0f || rectB_rotation != 0.0f) {
		auto rectA_polygon{ GetVertices(
			{ rectA_center, rectA_rotation }, rectA_size, Origin::Center, rectA_rotation_center
		) };
		auto rectB_polygon{ GetVertices(
			{ rectB_center, rectB_rotation }, rectB_size, Origin::Center, rectB_rotation_center
		) };
		return IntersectPolygonPolygon(
			rectA_polygon.data(), rectA_polygon.size(), rectB_polygon.data(), rectB_polygon.size()
		);
	}

#ifdef PTGN_DEBUG
	game.stats.intersect_rect_rect++;
#endif

	V2_float a_h{ rectA_size * 0.5f };
	V2_float b_h{ rectB_size * 0.5f };
	V2_float d{ rectB_center - rectA_center };
	V2_float pen{ a_h + b_h - V2_float{ Abs(d.x), Abs(d.y) } };

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
		c.depth	   = Abs(pen.y);
	} else {
		c.normal.x = -Sign(d.x);
		c.depth	   = Abs(pen.x);
	}

	PTGN_ASSERT(c.depth >= 0.0f);

	return c;
}

Intersection IntersectPolygonPolygon(
	const V2_float* pAv, std::size_t pA_vertex_count, const V2_float* pBv,
	std::size_t pB_vertex_count
) {
#ifdef PTGN_DEBUG
	game.stats.intersect_polygon_polygon++;
#endif
	PTGN_ASSERT(
		impl::IsConvexPolygon(pAv, pA_vertex_count) && impl::IsConvexPolygon(pBv, pB_vertex_count),
		"PolygonPolygon intersection check only works if both polygons are convex"
	);

	Intersection c;

	float depth{ std::numeric_limits<float>::infinity() };
	Axis axis;

	if (!impl::GetPolygonMinimumOverlap(pAv, pA_vertex_count, pBv, pB_vertex_count, depth, axis) ||
		!impl::GetPolygonMinimumOverlap(pBv, pB_vertex_count, pAv, pA_vertex_count, depth, axis)) {
		return c;
	}

	PTGN_ASSERT(depth != std::numeric_limits<float>::infinity());
	PTGN_ASSERT(depth >= 0.0f);

	// Make sure the vector is pointing from polygon1 to polygon2.
	if (V2_float dir{ GetCenter(pAv, pA_vertex_count) - GetCenter(pBv, pB_vertex_count) };
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

} // namespace impl

/*

// TODO: Readd somehow otherwise.
Intersection Intersects(const Transform& a, Circle A, const Transform& b, Circle B) {
	A.radius *= a.scale.x;
	B.radius *= b.scale.x;
	return impl::IntersectCircleCircle(a.position, A.radius, b.position, B.radius);
}

Intersection Intersects(const Transform& a, Circle A, Transform b, Rect B) {
	A.radius   *= a.scale.x;
	B.size	   *= b.scale;
	b.position += GetOriginOffset(B.origin, B.size);
	return impl::IntersectCircleRect(a.position, A.radius, b.position, B.size);
}

Intersection Intersects(const Transform& a, const Rect& A, const Transform& b, const Circle& B) {
	auto intersection{ Intersects(b, B, a, A) };
	intersection.normal *= -1.0f;
	return intersection;
}

Intersection Intersects(Transform a, Rect A, Transform b, Rect B) {
	A.size	   *= a.scale;
	a.position += GetOriginOffset(A.origin, A.size);
	B.size	   *= b.scale;
	b.position += GetOriginOffset(B.origin, B.size);
	return impl::IntersectRectRect(a.position, A.size, a.rotation, b.position, B.size, b.rotation);
}

Intersection Intersects(const Transform& a, Polygon A, const Transform& b, Polygon B) {
	for (auto& v : A.vertices) {
		v *= a.scale;
		v += a.position;
	}
	for (auto& v : B.vertices) {
		v *= b.scale;
		v += b.position;
	}
	return impl::IntersectPolygonPolygon(
		A.vertices.data(), A.vertices.size(), B.vertices.data(), B.vertices.size()
	);
}

*/

} // namespace ptgn