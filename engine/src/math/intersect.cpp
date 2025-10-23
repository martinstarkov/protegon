#include "math/intersect.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>
#include <variant>
#include <vector>

#include "core/app/game.h"
#include "core/ecs/components/transform.h"
#include "core/utils/type_info.h"
#include "debug/core/debug_config.h"
#include "debug/core/log.h"
#include "debug/runtime/assert.h"
#include "debug/runtime/debug_system.h"
#include "debug/runtime/stats.h"
#include "geometry/circle.h"
#include "geometry/polygon.h"
#include "geometry/rect.h"
#include "math/geometry/axis.h"
#include "math/geometry/shape.h"
#include "math/geometry_utils.h"
#include "math/math_utils.h"
#include "math/overlap.h"
#include "math/vector2.h"

#define PTGN_HANDLE_INTERSECT_SOLO_PAIR(TypeA, TypeB, PREFIX)               \
	if constexpr (std::is_same_v<S1, TypeA> && std::is_same_v<S2, TypeB>) { \
		return impl::PREFIX##TypeA##TypeB(t1, s1, t2, s2);                  \
	} else

#define PTGN_HANDLE_INTERSECT_PAIR(TypeA, TypeB, PREFIX)                           \
	if constexpr (std::is_same_v<S1, TypeA> && std::is_same_v<S2, TypeB>) {        \
		return impl::PREFIX##TypeA##TypeB(t1, s1, t2, s2);                         \
	} else if constexpr (std::is_same_v<S1, TypeB> && std::is_same_v<S2, TypeA>) { \
		return impl::PREFIX##TypeA##TypeB(t2, s2, t1, s1);                         \
	} else

#define PTGN_INTERSECT_SHAPE_PAIR_TABLE                        \
	PTGN_HANDLE_INTERSECT_SOLO_PAIR(Circle, Circle, Intersect) \
	PTGN_HANDLE_INTERSECT_PAIR(Circle, Rect, Intersect)        \
	PTGN_HANDLE_INTERSECT_PAIR(Circle, Polygon, Intersect)     \
	PTGN_HANDLE_INTERSECT_SOLO_PAIR(Rect, Rect, Intersect)     \
	PTGN_HANDLE_INTERSECT_SOLO_PAIR(Polygon, Polygon, Intersect)

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
	const Transform& t1, const Circle& A, const Transform& t2, const Circle& B
) {
	Intersection c;

	auto circleA_center{ A.GetCenter(t1) };
	auto circleB_center{ B.GetCenter(t2) };

	auto circleA_radius{ A.GetRadius(t1) };
	auto circleB_radius{ B.GetRadius(t2) };

	V2_float d{ circleB_center - circleA_center };
	float dist2{ d.Dot(d) };
	float r{ circleA_radius + circleB_radius };

	// No overlap.
	if (!impl::WithinPerimeter(r, dist2)) {
#ifdef PTGN_DEBUG
		game.debug.stats.overlap_circle_circle++;
#endif
		return c;
	}

#ifdef PTGN_DEBUG
	game.debug.stats.intersect_circle_circle++;
#endif

	if (dist2 > epsilon<float> * epsilon<float>) {
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
	const Transform& t1, const Circle& A, const Transform& t2, const Rect& B
) {
	if (t2.GetRotation() != 0.0f) {
		return IntersectCirclePolygon(t1, A, t2, Polygon{ B.GetLocalVertices() });
	}

#ifdef PTGN_DEBUG
	game.debug.stats.intersect_circle_rect++;
#endif
	// Source:
	// https://steamcdn-a.akamaihd.net/apps/valve/2015/DirkGregorius_Contacts.pdf
	Intersection c;

	auto circle_center{ A.GetCenter(t1) };
	auto circle_radius{ A.GetRadius(t1) };

	auto rect_center{ B.GetCenter(t2) };
	auto rect_size{ B.GetSize(t2) };

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
	const Transform& t1, const Circle& A, const Transform& t2, const Polygon& B
) {
	Intersection c;
#ifdef PTGN_DEBUG
	game.debug.stats.intersect_circle_polygon++;
#endif

	float min_penetration{ std::numeric_limits<float>::infinity() };
	V2_float collision_normal;

	auto polygon_vertices{ B.GetWorldVertices(t2) };
	std::size_t polygon_vertex_count{ polygon_vertices.size() };

	auto circle_radius{ A.GetRadius(t1) };
	auto circle_center{ A.GetCenter(t1) };

	// Check each edge of the polygon
	for (std::size_t i{ 0 }; i < polygon_vertex_count; ++i) {
		const V2_float& a{ polygon_vertices[i] };
		const V2_float& b{ polygon_vertices[(i + 1) % polygon_vertex_count] };
		V2_float edge{ b - a };
		V2_float edge_normal{ edge.Skewed().Normalized() }; // outward normal

		// Project circle center onto edge normal
		float distance_to_edge{ edge_normal.Dot(circle_center - a) };

		if (distance_to_edge > circle_radius) {
			// No intersection circle is outside
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
	const Transform& t1, const Rect& A, const Transform& t2, const Rect& B
) {
	Intersection c;

	if (t1.GetRotation() != 0.0f || t2.GetRotation() != 0.0f) {
		return IntersectPolygonPolygon(
			t1, Polygon{ A.GetLocalVertices() }, t2, Polygon{ B.GetLocalVertices() }
		);
	}

#ifdef PTGN_DEBUG
	game.debug.stats.intersect_rect_rect++;
#endif

	auto rectA_center{ A.GetCenter(t1) };
	auto rectA_size{ A.GetSize(t1) };

	auto rectB_center{ B.GetCenter(t2) };
	auto rectB_size{ B.GetSize(t2) };

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
	const Transform& t1, const Polygon& A, const Transform& t2, const Polygon& B
) {
#ifdef PTGN_DEBUG
	game.debug.stats.intersect_polygon_polygon++;
#endif

	Polygon polygon_A{ A.GetWorldVertices(t1) };
	Polygon polygon_B{ B.GetWorldVertices(t2) };

	PTGN_ASSERT(
		impl::IsConvexPolygon(polygon_A.vertices.data(), polygon_A.vertices.size()),
		"PolygonPolygon intersection check only works if both polygons are convex"
	);

	PTGN_ASSERT(
		impl::IsConvexPolygon(polygon_B.vertices.data(), polygon_B.vertices.size()),
		"PolygonPolygon intersection check only works if both polygons are convex"
	);

	Intersection c;

	float depth{ std::numeric_limits<float>::infinity() };
	Axis axis;

	if (!impl::GetPolygonMinimumOverlap(
			Transform{}, polygon_A, Transform{}, polygon_B, depth, axis
		) ||
		!impl::GetPolygonMinimumOverlap(
			Transform{}, polygon_B, Transform{}, polygon_A, depth, axis
		)) {
		return c;
	}

	PTGN_ASSERT(depth != std::numeric_limits<float>::infinity());
	PTGN_ASSERT(depth >= 0.0f);

	// Make sure the vector is pointing from polygon1 to polygon2.
	if (V2_float dir{ polygon_A.GetCenter() - polygon_B.GetCenter() };
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

Intersection Intersect(
	const Transform& t1, const ColliderShape& shape1, const Transform& t2,
	const ColliderShape& shape2
) {
	return std::visit(
		[&](const auto& s1) -> Intersection {
			return std::visit(
				[&](const auto& s2) -> Intersection {
					using S1 = std::decay_t<decltype(s1)>;
					using S2 = std::decay_t<decltype(s2)>;
					PTGN_INTERSECT_SHAPE_PAIR_TABLE {
						PTGN_ERROR(
							"Cannot find intersect function for the given shapes: ",
							type_name<S1>(), " and ", type_name<S2>()
						);
					}
				},
				shape2
			);
		},
		shape1
	);
}

} // namespace ptgn