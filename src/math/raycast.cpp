#include "math/raycast.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "common/assert.h"
#include "components/transform.h"
#include "core/game.h"
#include "debug/debugging.h"
#include "debug/log.h"
#include "debug/stats.h"
#include "geometry.h"
#include "geometry/capsule.h"
#include "geometry/circle.h"
#include "geometry/line.h"
#include "geometry/polygon.h"
#include "geometry/rect.h"
#include "math/math.h"
#include "math/overlap.h"
#include "math/utility.h"
#include "math/vector2.h"

#define PTGN_HANDLE_RAYCAST_LINE(TypeA, TypeB, PREFIX)                                    \
	if constexpr (std::is_same_v<S1, TypeA> && std::is_same_v<S2, TypeB>) {               \
		return impl::PREFIX##TypeB(V2_float{ s1 }, V2_float{ s1 } + ray, transform2, s2); \
	} else

#define PTGN_HANDLE_RAYCAST_SOLO_PAIR(TypeA, TypeB, PREFIX)                     \
	if constexpr (std::is_same_v<S1, TypeA> && std::is_same_v<S2, TypeB>) {     \
		return impl::PREFIX##TypeA##TypeB(ray, transform1, s1, transform2, s2); \
	} else

#define PTGN_RAYCAST_SHAPE_PAIR_TABLE                       \
	PTGN_HANDLE_RAYCAST_LINE(Point, Line, Raycast)          \
	PTGN_HANDLE_RAYCAST_LINE(Point, Circle, Raycast)        \
	PTGN_HANDLE_RAYCAST_LINE(Point, Rect, Raycast)          \
	PTGN_HANDLE_RAYCAST_LINE(Point, Capsule, Raycast)       \
	PTGN_HANDLE_RAYCAST_LINE(Point, Polygon, Raycast)       \
	PTGN_HANDLE_RAYCAST_SOLO_PAIR(Circle, Line, Raycast)    \
	PTGN_HANDLE_RAYCAST_SOLO_PAIR(Circle, Circle, Raycast)  \
	PTGN_HANDLE_RAYCAST_SOLO_PAIR(Circle, Rect, Raycast)    \
	PTGN_HANDLE_RAYCAST_SOLO_PAIR(Circle, Capsule, Raycast) \
	PTGN_HANDLE_RAYCAST_SOLO_PAIR(Circle, Polygon, Raycast) \
	PTGN_HANDLE_RAYCAST_SOLO_PAIR(Rect, Circle, Raycast)    \
	PTGN_HANDLE_RAYCAST_SOLO_PAIR(Rect, Rect, Raycast)      \
	PTGN_HANDLE_RAYCAST_SOLO_PAIR(Rect, Polygon, Raycast)   \
	PTGN_HANDLE_RAYCAST_SOLO_PAIR(Capsule, Circle, Raycast) \
	PTGN_HANDLE_RAYCAST_SOLO_PAIR(Polygon, Polygon, Raycast)

namespace ptgn {

bool RaycastResult::Occurred() const {
	PTGN_ASSERT(t >= 0.0f);
	return t >= 0.0f && t < 1.0f && !normal.IsZero();
}

namespace impl {

RaycastResult RaycastLine(
	const V2_float& ray_start, const V2_float& ray_end, const Transform& t2, const Line& B
) {
#ifdef PTGN_DEBUG
	game.stats.raycast_line_line++;
#endif
	// Source:
	// https://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect/565282#565282

	RaycastResult c;

	// TODO: Move to using a general overlap check.
	if (!OverlapLineLine(Transform{}, Line{ ray_start, ray_end }, t2, B)) {
		return c;
	}

	auto points{ B.GetWorldVertices(t2) };

	V2_float r{ ray_end - ray_start };
	V2_float s{ points[1] - points[0] };

	float sr{ s.Cross(r) };
	if (NearlyEqual(sr, 0.0f)) {
		return c;
	}

	V2_float ab{ ray_start - points[0] };
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

RaycastResult RaycastCircle(
	const V2_float& ray_start, const V2_float& ray_end, const Transform& transform2, const Circle& B
) {
#ifdef PTGN_DEBUG
	game.stats.raycast_line_circle++;
#endif
	// Source:
	// https://stackoverflow.com/questions/1073336/circle-line-segment-collision-detection-algorithm/1084899#1084899

	RaycastResult c;

	if (!OverlapLineCircle(Transform{}, Line{ ray_start, ray_end }, transform2, B)) {
		return c;
	}

	float circle_radius{ B.GetRadius(transform2) };
	auto circle_center{ B.GetCenter(transform2) };

	V2_float d{ -(ray_end - ray_start) };
	V2_float f{ circle_center - ray_start };

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

	V2_float impact{ circle_center + d * c.t - ray_start };

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

RaycastResult RaycastRect(
	const V2_float& ray_start, const V2_float& ray_end, const Transform& transform2, const Rect& B
) {
#ifdef PTGN_DEBUG
	game.stats.raycast_line_rect++;
#endif
	RaycastResult c;

	bool start_in{ OverlapPointRect(Transform{}, ray_start, transform2, B) };

	bool end_in{ OverlapPointRect(Transform{}, ray_end, transform2, B) };

	if (start_in && end_in) {
		return c;
	}

	V2_float d{ ray_end - ray_start };

	if (d.Dot(d) == 0.0f) {
		return c;
	}

	if (transform2.rotation != 0.0f) {
		return RaycastPolygon(ray_start, ray_end, transform2, Polygon{ B.GetLocalVertices() });
	}

	auto rect_size{ B.GetSize(transform2) };
	auto rect_center{ B.GetCenter(transform2) };

	V2_float inv_dir{ 1.0f / d };

	auto half{ rect_size * 0.5f };
	auto rect_min{ rect_center - half };
	auto rect_max{ rect_center + half };

	// Calculate intersections with rectangle bounding axes.
	V2_float near{ rect_min - ray_start };
	V2_float far{ rect_max - ray_start };

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

RaycastResult RaycastCapsule(
	const V2_float& ray_start, const V2_float& ray_end, const Transform& transform2,
	const Capsule& B
) {
#ifdef PTGN_DEBUG
	game.stats.raycast_line_capsule++;
#endif
	// Source: https://stackoverflow.com/a/52462458

	RaycastResult c;

	// TODO: Add early exit if overlap test fails.
	auto world_points{ B.GetWorldVertices(transform2) };

	V2_float cv{ world_points[1] - world_points[0] };
	float mag2{ cv.Dot(cv) };

	float capsule_radius{ B.GetRadius(transform2) };

	if (NearlyEqual(mag2, 0.0f)) {
		return RaycastCircle(
			ray_start, ray_end, Transform{ world_points[0] }, Circle{ capsule_radius }
		);
	}

	float mag{ std::sqrt(mag2) };
	V2_float cu{ cv / mag };
	// Normal to b.line
	V2_float ncu{ cu.Skewed() };
	V2_float ncu_dist{ ncu * capsule_radius };

	RaycastResult col_min{ c };

	auto c1{ RaycastLine(
		ray_start, ray_end, Transform{},
		Line{ world_points[0] + ncu_dist, world_points[1] + ncu_dist }
	) };
	auto c2{ RaycastLine(
		ray_start, ray_end, Transform{},
		Line{ world_points[0] - ncu_dist, world_points[1] - ncu_dist }
	) };
	auto c3{
		RaycastCircle(ray_start, ray_end, Transform{ world_points[0] }, Circle{ capsule_radius })
	};
	auto c4{
		RaycastCircle(ray_start, ray_end, Transform{ world_points[1] }, Circle{ capsule_radius })
	};

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

[[nodiscard]] RaycastResult RaycastPolygon(
	const V2_float& ray_start, const V2_float& ray_end, const Transform& transform2,
	const Polygon& B
) {
	PTGN_ASSERT(impl::IsConvexPolygon(B.vertices.data(), B.vertices.size()));
	// Convert polygon to world space
	auto world_points{ B.GetWorldVertices(transform2) };
	std::size_t count{ world_points.size() };

	PTGN_ASSERT(count > 2, "Polygon must have at least 3 vertices");

	RaycastResult closest;

	for (std::size_t i{ 0 }; i < count; ++i) {
		std::size_t j{ (i + 1) % count }; // wrap around to form a closed loop

		Line edge{ world_points[i], world_points[j] };

		RaycastResult hit{ RaycastLine(ray_start, ray_end, {}, edge) };

		if (hit.Occurred() && hit.t < closest.t) {
			closest = hit;
		}
	}

	return closest;
}

RaycastResult RaycastCircleLine(
	const V2_float& ray, const Transform& transform1, const Circle& A, const Transform& transform2,
	const Line& B
) {
	auto circle_center{ A.GetCenter(transform1) };
	return RaycastCapsule(
		circle_center, circle_center + ray, transform2,
		Capsule{ B.start, B.end, A.GetRadius(transform1) }
	);
}

[[nodiscard]] RaycastResult RaycastCirclePolygon(
	const V2_float& ray, const Transform& transform1, const Circle& A, const Transform& transform2,
	const Polygon& B
) {
	PTGN_ASSERT(impl::IsConvexPolygon(B.vertices.data(), B.vertices.size()));
	// Convert polygon to world space
	auto world_points{ B.GetWorldVertices(transform2) };
	std::size_t count{ world_points.size() };

	PTGN_ASSERT(count > 2, "Polygon must have at least 3 vertices");

	RaycastResult closest;

	for (std::size_t i{ 0 }; i < count; ++i) {
		std::size_t j{ (i + 1) % count }; // wrap around to form a closed loop

		Line edge{ world_points[i], world_points[j] };

		RaycastResult hit{ RaycastCircleLine(ray, transform1, A, {}, edge) };

		if (hit.Occurred() && hit.t < closest.t) {
			closest = hit;
		}
	}

	return closest;
}

RaycastResult RaycastCircleCircle(
	const V2_float& ray, const Transform& transform1, const Circle& A, const Transform& transform2,
	const Circle& B
) {
	auto circleA_center{ A.GetCenter(transform1) };
	auto circleB_center{ B.GetCenter(transform2) };
	return RaycastCircle(
		circleA_center, circleA_center + ray, Transform{ circleB_center, transform2.rotation },
		Circle{ A.GetRadius(transform1) + B.GetRadius(transform2) }
	);
}

RaycastResult RaycastCircleRect(
	const V2_float& ray, const Transform& transform1, const Circle& A, const Transform& transform2,
	const Rect& B
) {
#ifdef PTGN_DEBUG
	game.stats.raycast_circle_rect++;
#endif
	if (transform2.rotation != 0.0f) {
		return RaycastCirclePolygon(
			ray, transform1, A, transform2, Polygon{ B.GetLocalVertices() }
		);
	}

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

	auto circle_center{ A.GetCenter(transform1) };
	auto circle_radius{ A.GetRadius(transform1) };
	auto rect_size{ B.GetSize(transform2) };
	auto rect_center{ B.GetCenter(transform2) };

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

	V2_float half{ rect_size * 0.5f };
	V2_float top_left{ rect_center - half };
	V2_float bottom_right{ rect_center + half };
	V2_float top_right{ bottom_right.x, top_left.y };
	V2_float bottom_left{ top_left.x, bottom_right.y };

	const auto raycast_capsule_segment = [&](const V2_float& start, const V2_float& end) {
		auto collision{ RaycastCapsule(
			circle_center, ray_end, Transform{}, Capsule{ start, end, circle_radius }
		) };
		if (collision.Occurred() && collision.t < col_min.t) {
			col_min = collision;
		}
	};

	std::invoke(raycast_capsule_segment, top_left, top_right);		 // Top segment.
	std::invoke(raycast_capsule_segment, top_right, bottom_right);	 // Right segment.
	std::invoke(raycast_capsule_segment, bottom_right, bottom_left); // Bottom segment.
	std::invoke(raycast_capsule_segment, bottom_left, top_left);	 // Left segment.

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
	const V2_float& ray, const Transform& transform1, const Circle& A, const Transform& transform2,
	const Capsule& B
) {
	auto circle_center{ A.GetCenter(transform1) };
	auto capsule_center{ transform2.position };
	return RaycastCapsule(
		circle_center, circle_center + ray, Transform{ capsule_center, transform2.rotation },
		Capsule{ B.start, B.end, A.GetRadius(transform1) + B.GetRadius(transform2) }
	);
}

RaycastResult RaycastRectCircle(
	const V2_float& ray, const Transform& transform1, const Rect& A, const Transform& transform2,
	const Circle& B
) {
	return RaycastCircleRect(-ray, transform2, B, transform1, A);
}

RaycastResult RaycastRectRect(
	const V2_float& ray, const Transform& transform1, const Rect& A, const Transform& transform2,
	const Rect& B
) {
#ifdef PTGN_DEBUG
	game.stats.raycast_rect_rect++;
#endif
	bool rotated1{ transform1.rotation != 0.0f };
	bool rotated2{ transform2.rotation != 0.0f };

	if (!rotated1 && !rotated2 || !rotated1 && rotated2) {
		auto rectA_center{ A.GetCenter(transform1) };
		auto rectB_center{ B.GetCenter(transform2) };
		return RaycastRect(
			rectA_center, rectA_center + ray, Transform{ rectB_center, transform2.rotation },
			Rect{ A.GetSize(transform1) + B.GetSize(transform2) }
		);
	} else if (rotated1 && !rotated2) {
		return RaycastRectRect(-ray, transform2, B, transform1, A);
	} else {
		return RaycastPolygonPolygon(
			ray, transform1, Polygon{ A.GetLocalVertices() }, transform2,
			Polygon{ B.GetLocalVertices() }
		);
	}
}

RaycastResult RaycastRectPolygon(
	const V2_float& ray, const Transform& transform1, const Rect& A, const Transform& transform2,
	const Polygon& B
) {
	return RaycastPolygonPolygon(ray, transform1, Polygon{ A.GetLocalVertices() }, transform2, B);
}

RaycastResult RaycastPolygonPolygon(
	const V2_float& ray, const Transform& transform1, const Polygon& A, const Transform& transform2,
	const Polygon& B
) {
	PTGN_ASSERT(impl::IsConvexPolygon(A.vertices.data(), A.vertices.size()));
	PTGN_ASSERT(impl::IsConvexPolygon(B.vertices.data(), B.vertices.size()));

	RaycastResult best;

	auto worldA{ A.GetWorldVertices(transform1) };
	auto worldB{ B.GetWorldVertices(transform2) };

	const auto sweep = [&](const std::vector<V2_float>& verts, const std::vector<Line>& edges,
						   const V2_float& swep_vel) {
		for (const auto& v : verts) {
			for (const auto& edge : edges) {
				RaycastResult res{ RaycastLine(v, v + swep_vel, Transform{}, edge) };
				if (!res.Occurred()) {
					continue;
				}
				if (res.t < best.t) {
					best = res;
				}
			}
		}
	};

	// Build edges of polygons
	auto get_edges = [](const std::vector<V2_float>& pts) {
		std::vector<Line> edges;
		size_t n = pts.size();
		if (n < 2) {
			return edges;
		}
		edges.reserve(n);
		for (size_t i = 0; i < n; ++i) {
			edges.push_back({ pts[i], pts[(i + 1) % n] });
		}
		return edges;
	};

	auto edgesB = get_edges(worldB);
	sweep(worldA, edgesB, ray);

	// Also sweep vertices of B backwards against edges of A
	auto edgesA = get_edges(worldA);
	sweep(worldB, edgesA, -ray);

	return best;
}

RaycastResult RaycastCapsuleCircle(
	const V2_float& ray, const Transform& transform1, const Capsule& A, const Transform& transform2,
	const Circle& B
) {
	return RaycastCircleCapsule(-ray, transform2, B, transform1, A);
}

} // namespace impl

RaycastResult Raycast(
	const V2_float& ray, const Transform& transform1, const Shape& shape1,
	const Transform& transform2, const Shape& shape2
) {
	return std::visit(
		[&](const auto& s1) -> RaycastResult {
			return std::visit(
				[&](const auto& s2) -> RaycastResult {
					using S1 = std::decay_t<decltype(s1)>;
					using S2 = std::decay_t<decltype(s2)>;
					PTGN_RAYCAST_SHAPE_PAIR_TABLE {
						PTGN_ERROR("Cannot find raycast function for the given shapes");
					}
				},
				shape2
			);
		},
		shape1
	);
}

} // namespace ptgn