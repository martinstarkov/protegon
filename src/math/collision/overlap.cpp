#include "math/collision/overlap.h"

#include <array>
#include <limits>
#include <utility>

#include "components/transform.h"
#include "core/game.h"
#include "math/geometry/axis.h"
#include "math/geometry/circle.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/math.h"
#include "math/utility.h"
#include "math/vector2.h"
#include "renderer/origin.h"
#include "utility/assert.h"
#include "utility/debug.h"

namespace ptgn {

namespace impl {

bool PolygonsHaveOverlapAxis(
	const V2_float* pAv, std::size_t pA_vertex_count, const V2_float* pBv,
	std::size_t pB_vertex_count
) {
	const auto axes{ impl::GetPolygonAxes(pAv, pA_vertex_count, false) };
	for (const Axis& a : axes) {
		auto [min1, max1] = impl::GetPolygonProjectionMinMax(pAv, pA_vertex_count, a);
		auto [min2, max2] = impl::GetPolygonProjectionMinMax(pBv, pB_vertex_count, a);

		if (!impl::IntervalsOverlap(min1, max1, min2, max2)) {
			return false;
		}
	}
	return true;
}

bool GetPolygonMinimumOverlap(
	const V2_float* pAv, std::size_t pA_vertex_count, const V2_float* pBv,
	std::size_t pB_vertex_count, float& depth, Axis& axis
) {
	const auto axes{ impl::GetPolygonAxes(pAv, pA_vertex_count, true) };
	for (const Axis& a : axes) {
		auto [min1, max1] = impl::GetPolygonProjectionMinMax(pAv, pA_vertex_count, a);
		auto [min2, max2] = impl::GetPolygonProjectionMinMax(pBv, pB_vertex_count, a);

		if (!impl::IntervalsOverlap(min1, max1, min2, max2)) {
			return false;
		}
		bool contained{ PolygonContainsPolygon(pAv, pA_vertex_count, pBv, pB_vertex_count) ||
						PolygonContainsPolygon(pBv, pB_vertex_count, pAv, pA_vertex_count) };

		float o{ impl::GetIntervalOverlap(min1, max1, min2, max2, contained, axis.direction) };

		if (o < depth) {
			depth = o;
			axis  = a;
		}
	}
	return true;
}

bool LineContainsLine(
	const V2_float& lineA_start, const V2_float& lineA_end, const V2_float& lineB_start,
	const V2_float& lineB_end
) {
	if (auto d{ (lineA_end - lineA_start).Cross(lineB_end - lineB_start) }; !NearlyEqual(d, 0.0f)) {
		return false;
	}

	float a1{
		impl::ParallelogramArea(lineA_start, lineA_end, lineB_end)
	}; // Compute winding of abd (+ or -)
	float a2{ impl::ParallelogramArea(lineA_start, lineA_end, lineB_start) };

	if (bool collinear{ NearlyEqual(a1, 0.0f) || NearlyEqual(a2, 0.0f) }; !collinear) {
		return false;
	}

	if (OverlapPointLine(lineB_start, lineA_start, lineA_end) &&
		OverlapPointLine(lineB_end, lineA_start, lineA_end)) {
		return true;
	}

	return false;
}

bool PolygonContainsPolygon(
	const V2_float* pAv, std::size_t pA_vertex_count, const V2_float* pBv,
	std::size_t pB_vertex_count
) {
	for (std::size_t i{ 0 }; i < pB_vertex_count; i++) {
		const auto& point{ pBv[i] };
		if (!OverlapPointPolygon(point, pAv, pA_vertex_count)) {
			return false;
		}
	}
	return true;
}

bool TriangleContainsTriangle(
	const V2_float& v1A, const V2_float& v2A, const V2_float& v3A, const V2_float& v1B,
	const V2_float& v2B, const V2_float& v3B
) {
	return OverlapPointTriangle(v1B, v1A, v2A, v3A) && OverlapPointTriangle(v2B, v1A, v2A, v3A) &&
		   OverlapPointTriangle(v3B, v1A, v2A, v3A);
}

bool PolygonContainsTriangle(
	const V2_float* vertices, std::size_t vertex_count, const V2_float& triangle_a,
	const V2_float& triangle_b, const V2_float& triangle_c
) {
	return OverlapPointPolygon(triangle_a, vertices, vertex_count) &&
		   OverlapPointPolygon(triangle_b, vertices, vertex_count) &&
		   OverlapPointPolygon(triangle_c, vertices, vertex_count);
}

bool OverlapPointLine(const V2_float& point, const V2_float& line_start, const V2_float& line_end) {
#ifdef PTGN_DEBUG
	game.stats.overlap_point_line++;
#endif
	// Source:
	// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 130. (SqDistPointSegment == 0) but optimized.

	V2_float ab{ line_end - line_start };
	V2_float ac{ point - line_start };
	V2_float bc{ point - line_end };

	float e{ ac.Dot(ab) };
	// Handle cases where c projects outside ab.
	if (e < 0 || NearlyEqual(e, 0.0f)) {
		return NearlyEqual(ac.x, 0.0f) && NearlyEqual(ac.y, 0.0f);
	}

	float f{ ab.Dot(ab) };
	if (e > f || NearlyEqual(e, f)) {
		return NearlyEqual(bc.x, 0.0f) && NearlyEqual(bc.y, 0.0f);
	}

	// Handle cases where c projects onto ab.
	return NearlyEqual(ac.Dot(ac) * f, e * e);
}

bool OverlapPointTriangle(
	const V2_float& point, const V2_float& triangle_a, const V2_float& triangle_b,
	const V2_float& triangle_c
) {
#ifdef PTGN_DEBUG
	game.stats.overlap_point_triangle++;
#endif
	// Using barycentric coordinates method.
	float A{ 0.5f * (-triangle_b.y * triangle_c.x + triangle_a.y * (-triangle_b.x + triangle_c.x) +
					 triangle_a.x * (triangle_b.y - triangle_c.y) + triangle_b.x * triangle_c.y) };
	float z{ 1.0f / (2.0f * A) };
	float s{ z *
			 (triangle_a.y * triangle_c.x - triangle_a.x * triangle_c.y +
			  (triangle_c.y - triangle_a.y) * point.x + (triangle_a.x - triangle_c.x) * point.y) };
	float t{ z *
			 (triangle_a.x * triangle_b.y - triangle_a.y * triangle_b.x +
			  (triangle_a.y - triangle_b.y) * point.x + (triangle_b.x - triangle_a.x) * point.y) };

	return s >= 0.0f && t >= 0.0f && (s + t) <= 1.0f;
}

bool OverlapPointCircle(const V2_float& point, const V2_float& circle_center, float circle_radius) {
#ifdef PTGN_DEBUG
	game.stats.overlap_point_circle++;
#endif
	V2_float dist{ circle_center - point };
	return impl::WithinPerimeter(circle_radius, dist.Dot(dist));
}

bool OverlapPointRect(
	const V2_float& point, const V2_float& rect_center, const V2_float& rect_size,
	float rect_rotation
) {
#ifdef PTGN_DEBUG
	game.stats.overlap_point_rect++;
#endif
	if (rect_rotation != 0.0f) {
		auto rect_polygon{
			impl::GetVertices({ rect_center, rect_rotation }, { rect_size, Origin::Center })
		};
		return OverlapPointPolygon(point, rect_polygon.data(), rect_polygon.size());
	}

	auto half{ rect_size * 0.5f };
	auto rect_min{ rect_center - half };
	auto rect_max{ rect_center + half };

	if (point.x < rect_min.x || point.x > rect_max.x) {
		return false;
	}

	if (point.y < rect_min.y || point.y > rect_max.y) {
		return false;
	}

	if (NearlyEqual(point.x, rect_max.x) || NearlyEqual(point.x, rect_min.x)) {
		return false;
	}

	if (NearlyEqual(point.y, rect_min.y) || NearlyEqual(point.y, rect_max.y)) {
		return false;
	}

	return true;
}

bool OverlapPointCapsule(
	const V2_float& point, const V2_float& capsule_start, const V2_float& capsule_end,
	float capsule_radius
) {
#ifdef PTGN_DEBUG
	game.stats.overlap_point_capsule++;
#endif
	// Source:
	// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 114.
	return impl::WithinPerimeter(
		capsule_radius, impl::SquareDistancePointLine(point, capsule_start, capsule_end)
	);
}

bool OverlapPointPolygon(const V2_float& point, const V2_float* v, std::size_t count) {
#ifdef PTGN_DEBUG
	game.stats.overlap_point_polygon++;
#endif
	bool c{ false };
	std::size_t i{ 0 };
	std::size_t j{ count - 1 };
	// Algorithm from: https://wrfranklin.org/Research/Short_Notes/pnpoly.html
	for (; i < count; j = i++) {
		bool a{ (v[i].y > point.y) != (v[j].y > point.y) };
		bool b{ point.x < (v[j].x - v[i].x) * (point.y - v[i].y) / (v[j].y - v[i].y) + v[i].x };
		if (a && b) {
			c = !c;
		}
	}
	return c;
}

bool OverlapLineLine(
	const V2_float& lineA_start, const V2_float& lineA_end, const V2_float& lineB_start,
	const V2_float& lineB_end
) {
#ifdef PTGN_DEBUG
	game.stats.overlap_line_line++;
#endif
	// Source:
	// https://www.geeksforgeeks.org/check-if-two-given-line-segments-intersect/

	// Sign of areas correspond to which side of ab points c and d are
	float a1{
		impl::ParallelogramArea(lineA_start, lineA_end, lineB_end)
	}; // Compute winding of abd (+ or -)
	float a2{
		impl::ParallelogramArea(lineA_start, lineA_end, lineB_start)
	}; // To intersect, must have sign opposite of a1
	// If c and d are on different sides of ab, areas have different signs
	bool polarity_diff{ false };
	bool collinear{ false };
	// Same as above but for floating points.
	polarity_diff = a1 * a2 < 0.0f;
	collinear	  = NearlyEqual(a1, 0.0f) || NearlyEqual(a2, 0.0f);
	// For integral implementation use this instead of the above two lines:
	// if constexpr (std::is_signed_v<T> && std::is_integral_v<T>) {
	//	// Second part for difference in polarity.
	//	polarity_diff = (a1 ^ a2) < 0;
	//	collinear = a1 == 0 || a2 == 0;
	//}
	if (!collinear && polarity_diff) {
		// Compute signs for a and b with respect to segment cd
		float a3{
			impl::ParallelogramArea(lineB_start, lineB_end, lineA_start)
		}; // Compute winding of cda (+ or -)
		// Since area is constant a1 - a2 = a3 - a4, or a4 = a3 + a2 - a1
		// const T a4 = math::ParallelogramArea(c, d, b); // Must have opposite
		// sign of a3
		float a4{ a3 + a2 - a1 };
		// Points a and b on different sides of cd if areas have different signs
		// Segments intersect if true.
		bool intersect{ false };
		// If either is 0, the line is intersecting with the straight edge of
		// the other line. (i.e. corners with angles). Check if a3 and a4 signs
		// are different.
		intersect = a3 * a4 < 0.0f;
		// collinear = NearlyEqual(a3, 0.0f) || NearlyEqual(a4, 0.0f);

		//  For integral implementation use this instead of the above two lines:
		//  if constexpr (std::is_signed_v<T> && std::is_integral_v<T>) {
		//	intersect = (a3 ^ a4) < 0;
		//	collinear = a3 == 0 || a4 == 0;
		// }
		if (intersect) {
			return true;
		}
	}

	/*[[maybe_unused]] bool point_overlap{
		(Overlaps(lineB_end) || Overlaps(lineB_start) || line.Overlaps(lineA_start) ||
	line.Overlaps(lineA_end))
	};*/

	return false; // collinear && point_overlap;
}

bool OverlapLineCircle(
	const V2_float& line_start, const V2_float& line_end, const V2_float& circle_center,
	float circle_radius
) {
#ifdef PTGN_DEBUG
	game.stats.overlap_line_circle++;
#endif
	// Source: https://www.baeldung.com/cs/circle-line-segment-collision-detection

	// If the line is inside the circle entirely, exit early.
	if (OverlapPointCircle(line_start, circle_center, circle_radius) &&
		OverlapPointCircle(line_end, circle_center, circle_radius)) {
		return true;
	}

	float min_dist2{ std::numeric_limits<float>::infinity() };

	// O is the circle center, P is the line start, Q is the line end.
	V2_float OP{ line_start - circle_center };
	V2_float OQ{ line_end - circle_center };
	V2_float PQ{ line_end - line_start };

	float OP_dist2{ OP.Dot(OP) };
	float OQ_dist2{ OQ.Dot(OQ) };
	float max_dist2{ std::max(OP_dist2, OQ_dist2) };

	if (OP.Dot(-PQ) > 0.0f && OQ.Dot(PQ) > 0.0f) {
		float triangle_area{ FastAbs(impl::ParallelogramArea(circle_center, line_start, line_end)) /
							 2.0f };
		min_dist2 = 4.0f * triangle_area * triangle_area / PQ.Dot(PQ);
	} else {
		min_dist2 = std::min(OP_dist2, OQ_dist2);
	}

	return impl::WithinPerimeter(circle_radius, min_dist2) &&
		   !impl::WithinPerimeter(circle_radius, max_dist2);
}

bool OverlapLineTriangle(
	const V2_float& line_start, const V2_float& line_end, const V2_float& triangle_a,
	const V2_float& triangle_b, const V2_float& triangle_c
) {
	return OverlapPointTriangle(line_start, triangle_a, triangle_b, triangle_c) ||
		   OverlapPointTriangle(line_end, triangle_a, triangle_b, triangle_c) ||
		   OverlapLineLine(line_start, line_end, triangle_a, triangle_b) ||
		   OverlapLineLine(line_start, line_end, triangle_b, triangle_c) ||
		   OverlapLineLine(line_start, line_end, triangle_c, triangle_a);
}

bool OverlapLineRect(
	const V2_float& line_start, const V2_float& line_end, const V2_float& rect_center,
	const V2_float& rect_size
) {
#ifdef PTGN_DEBUG
	game.stats.overlap_line_rect++;
#endif
	// TODO: Add rotation check.

	V2_float c{ rect_center };
	V2_float e{ rect_size * 0.5f };
	V2_float m{ Midpoint(line_start, line_end) };
	V2_float d{ line_end - m }; // Line halflength vector

	m = m - c;					// Translate box and segment to origin

	// Try world coordinate axes as separating axes.
	float adx{ FastAbs(d.x) };
	if (FastAbs(m.x) >= e.x + adx) {
		return false;
	}

	float ady{ FastAbs(d.y) };
	if (FastAbs(m.y) >= e.y + ady) {
		return false;
	}

	// Add in an epsilon term to counteract arithmetic errors when segment is
	// (near) parallel to a coordinate axis.
	adx += epsilon<float>;
	ady += epsilon<float>;

	// Try cross products of segment direction vector with coordinate axes.
	float cross{ m.Cross(d) };

	if (float dot{ e.Dot({ ady, adx }) }; FastAbs(cross) > dot) {
		return false;
	}

	// No separating axis found; segment must be overlapping AABB.
	return true;

	// Alternative method:
	// Source: https://en.wikipedia.org/wiki/Cohen%E2%80%93Sutherland_algorithm
}

bool OverlapLineCapsule(
	const V2_float& line_start, const V2_float& line_end, const V2_float& capsule_start,
	const V2_float& capsule_end, float capsule_radius
) {
#ifdef PTGN_DEBUG
	game.stats.overlap_line_capsule++;
#endif
	// Source:
	// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 114-115.
	float s{ 0.0f };
	float t{ 0.0f };
	V2_float c1;
	V2_float c2;
	return impl::WithinPerimeter(
		capsule_radius,
		impl::ClosestPointLineLine(line_start, line_end, capsule_start, capsule_end, s, t, c1, c2)
	);
}

bool OverlapLinePolygon(
	const V2_float& line_start, const V2_float& line_end, const V2_float* polygon_vertices,
	std::size_t polygon_vertex_count
) {
	if (OverlapPointPolygon(line_start, polygon_vertices, polygon_vertex_count)) {
		return true;
	}

	PTGN_ASSERT(impl::IsConvexPolygon(polygon_vertices, polygon_vertex_count));

	for (std::size_t i{ 0 }; i < polygon_vertex_count; ++i) {
		if (OverlapLineLine(
				line_start, line_end, polygon_vertices[i],
				polygon_vertices[(i + 1) % polygon_vertex_count]
			)) {
			return true;
		}
	}
	return false;
}

bool OverlapCircleCircle(
	const V2_float& circleA_center, float circleA_radius, const V2_float& circleB_center,
	float circleB_radius
) {
#ifdef PTGN_DEBUG
	game.stats.overlap_circle_circle++;
#endif
	// Source:
	// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 88.
	V2_float dist{ circleA_center - circleB_center };

	return impl::WithinPerimeter(circleA_radius + circleB_radius, dist.Dot(dist));
}

bool OverlapCircleTriangle(
	const V2_float& circle_center, float circle_radius, const V2_float& triangle_a,
	const V2_float& triangle_b, const V2_float& triangle_c
) {
	return OverlapPointTriangle(circle_center, triangle_a, triangle_b, triangle_c) ||
		   OverlapLineCircle(triangle_a, triangle_b, circle_center, circle_radius) ||
		   OverlapLineCircle(triangle_b, triangle_c, circle_center, circle_radius) ||
		   OverlapLineCircle(triangle_c, triangle_b, circle_center, circle_radius);
}

bool OverlapCircleRect(
	const V2_float& circle_center, float circle_radius, const V2_float& rect_center,
	const V2_float& rect_size
) {
#ifdef PTGN_DEBUG
	game.stats.overlap_circle_rect++;
#endif
	// Source:
	// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 165-166.
	// TODO: Add rotation check.
	auto half{ rect_size * 0.5f };
	auto rect_min{ rect_center - half };
	auto rect_max{ rect_center + half };

	return impl::WithinPerimeter(
		circle_radius, impl::SquareDistancePointRect(circle_center, rect_min, rect_max)
	);
}

bool OverlapCirclePolygon(
	const V2_float& circle_center, float circle_radius, const V2_float* polygon_vertices,
	std::size_t polygon_vertex_count
) {
	if (OverlapPointPolygon(circle_center, polygon_vertices, polygon_vertex_count)) {
		return true;
	}

	PTGN_ASSERT(impl::IsConvexPolygon(polygon_vertices, polygon_vertex_count));

	for (std::size_t i{ 0 }; i < polygon_vertex_count; ++i) {
		if (OverlapLineCircle(
				polygon_vertices[i], polygon_vertices[(i + 1) % polygon_vertex_count],
				circle_center, circle_radius
			)) {
			return true;
		}
	}

	return false;
}

bool OverlapCircleCapsule(
	const V2_float& circle_center, float circle_radius, const V2_float& capsule_start,
	const V2_float& capsule_end, float capsule_radius
) {
#ifdef PTGN_DEBUG
	game.stats.overlap_circle_capsule++;
#endif
	// Source:
	// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 114.
	// If (squared) distance smaller than (squared) sum of radii, they collide
	return impl::WithinPerimeter(
		circle_radius + capsule_radius,
		impl::SquareDistancePointLine(circle_center, capsule_start, capsule_end)
	);
}

bool OverlapTriangleTriangle(
	const V2_float& triangleA_a, const V2_float& triangleA_b, const V2_float& triangleA_c,
	const V2_float& triangleB_a, const V2_float& triangleB_b, const V2_float& triangleB_c
) {
	return OverlapPointTriangle(triangleA_a, triangleB_a, triangleB_b, triangleB_c) ||
		   OverlapLineTriangle(triangleB_a, triangleB_b, triangleA_a, triangleA_b, triangleA_c) ||
		   OverlapLineTriangle(triangleB_b, triangleB_c, triangleA_a, triangleA_b, triangleA_c) ||
		   OverlapLineTriangle(triangleB_c, triangleB_a, triangleA_a, triangleA_b, triangleA_c);
}

bool OverlapTriangleRect(
	const V2_float& triangle_a, const V2_float& triangle_b, const V2_float& triangle_c,
	const V2_float& rect_center, const V2_float& rect_size, float rect_rotation
) {
#ifdef PTGN_DEBUG
	game.stats.overlap_triangle_rect++;
#endif
	auto rect_polygon{
		impl::GetVertices({ rect_center, rect_rotation }, { rect_size, Origin::Center })
	};
	std::array<V2_float, 3> triangle{ triangle_a, triangle_b, triangle_c };
	return OverlapPolygonPolygon(
		triangle.data(), triangle.size(), rect_polygon.data(), rect_polygon.size()
	);
}

bool OverlapTrianglePolygon(
	const V2_float& triangle_a, const V2_float& triangle_b, const V2_float& triangle_c,
	const V2_float* polygon_vertices, std::size_t polygon_vertex_count
) {
	if (OverlapPointPolygon(triangle_a, polygon_vertices, polygon_vertex_count)) {
		return true;
	}

	PTGN_ASSERT(impl::IsConvexPolygon(polygon_vertices, polygon_vertex_count));

	for (std::size_t i{ 0 }; i < polygon_vertex_count; ++i) {
		if (OverlapLineTriangle(
				polygon_vertices[i], polygon_vertices[(i + 1) % polygon_vertex_count], triangle_a,
				triangle_b, triangle_c
			)) {
			return true;
		}
	}

	return false;
}

bool OverlapRectRect(
	const V2_float& rectA_center, const V2_float& rectA_size, float rectA_rotation,
	const V2_float& rectB_center, const V2_float& rectB_size, float rectB_rotation
) {
	if (rectA_rotation != 0.0f || rectB_rotation != 0.0f) {
		auto rectA_polygon{
			impl::GetVertices({ rectA_center, rectA_rotation }, { rectA_size, Origin::Center })
		};
		auto rectB_polygon{
			impl::GetVertices({ rectB_center, rectB_rotation }, { rectB_size, Origin::Center })
		};
		return OverlapPolygonPolygon(
			rectA_polygon.data(), rectA_polygon.size(), rectB_polygon.data(), rectB_polygon.size()
		);
	}
#ifdef PTGN_DEBUG
	game.stats.overlap_rect_rect++;
#endif
	auto halfA{ rectA_size * 0.5f };
	auto rectA_min{ rectA_center - halfA };
	auto rectA_max{ rectA_center + halfA };
	auto halfB{ rectB_size * 0.5f };
	auto rectB_min{ rectB_center - halfB };
	auto rectB_max{ rectB_center + halfB };

	if (rectA_max.x < rectB_min.x || rectA_min.x > rectB_max.x) {
		return false;
	}

	if (rectA_max.y < rectB_min.y || rectA_min.y > rectB_max.y) {
		return false;
	}

	// Optional: Ignore seam collisions:

	if (NearlyEqual(rectA_min.x, rectB_max.x) || NearlyEqual(rectA_max.x, rectB_min.x)) {
		return false;
	}

	if (NearlyEqual(rectA_max.y, rectB_min.y) || NearlyEqual(rectA_min.y, rectB_max.y)) {
		return false;
	}

	return true;
}

bool OverlapRectCapsule(
	const V2_float& rect_center, const V2_float& rect_size, float rect_rotation,
	const V2_float& capsule_start, const V2_float& capsule_end, float capsule_radius
) {
#ifdef PTGN_DEBUG
	game.stats.overlap_rect_capsule++;
#endif
	if (OverlapPointRect(capsule_start, rect_center, rect_size, rect_rotation)) {
		return true;
	}
	if (OverlapPointRect(capsule_end, rect_center, rect_size, rect_rotation)) {
		return true;
	}

	// Rotated rect.
	if (rect_rotation != 0.0f) {
		auto rect_polygon{
			impl::GetVertices({ rect_center, rect_rotation }, { rect_size, Origin::Center })
		};
		if (OverlapLineCapsule(
				rect_polygon[0], rect_polygon[1], capsule_start, capsule_end, capsule_radius
			)) {
			return true;
		}
		if (OverlapLineCapsule(
				rect_polygon[1], rect_polygon[2], capsule_start, capsule_end, capsule_radius
			)) {
			return true;
		}
		if (OverlapLineCapsule(
				rect_polygon[2], rect_polygon[3], capsule_start, capsule_end, capsule_radius
			)) {
			return true;
		}
		if (OverlapLineCapsule(
				rect_polygon[3], rect_polygon[0], capsule_start, capsule_end, capsule_radius
			)) {
			return true;
		}
		return false;
	}
	auto half{ rect_size * 0.5f };
	auto rect_min{ rect_center - half };
	auto rect_max{ rect_center + half };
	// No rotation.
	if (OverlapLineCapsule(
			rect_min, { rect_max.x, rect_min.y }, capsule_start, capsule_end, capsule_radius
		)) {
		return true;
	}
	if (OverlapLineCapsule(
			{ rect_max.x, rect_min.y }, rect_max, capsule_start, capsule_end, capsule_radius
		)) {
		return true;
	}
	if (OverlapLineCapsule(
			rect_max, { rect_min.x, rect_max.y }, capsule_start, capsule_end, capsule_radius
		)) {
		return true;
	}
	if (OverlapLineCapsule(
			{ rect_min.x, rect_max.y }, rect_min, capsule_start, capsule_end, capsule_radius
		)) {
		return true;
	}
	return false;
}

bool OverlapRectPolygon(
	const V2_float& rect_center, const V2_float& rect_size, float rect_rotation,
	const V2_float* vertices, std::size_t vertex_count
) {
	auto rect_polygon{
		impl::GetVertices({ rect_center, rect_rotation }, { rect_size, Origin::Center })
	};
	return OverlapPolygonPolygon(rect_polygon.data(), rect_polygon.size(), vertices, vertex_count);
}

bool OverlapCapsuleCapsule(
	const V2_float& capsuleA_start, const V2_float& capsuleA_end, float capsuleA_radius,
	const V2_float& capsuleB_start, const V2_float& capsuleB_end, float capsuleB_radius
) {
#ifdef PTGN_DEBUG
	game.stats.overlap_capsule_capsule++;
#endif
	// Source:
	// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 114-115.
	float s{ 0.0f };
	float t{ 0.0f };
	V2_float c1;
	V2_float c2;
	return impl::WithinPerimeter(
		capsuleA_radius + capsuleB_radius,
		impl::ClosestPointLineLine(
			capsuleA_start, capsuleA_end, capsuleB_start, capsuleB_end, s, t, c1, c2
		)
	);
}

bool OverlapPolygonPolygon(
	const V2_float* pAv, std::size_t pA_vertex_count, const V2_float* pBv,
	std::size_t pB_vertex_count
) {
#ifdef PTGN_DEBUG
	game.stats.overlap_polygon_polygon++;
#endif
	PTGN_ASSERT(
		impl::IsConvexPolygon(pAv, pA_vertex_count) && impl::IsConvexPolygon(pBv, pB_vertex_count),
		"PolygonPolygon overlap check only works if both polygons are convex"
	);
	return impl::PolygonsHaveOverlapAxis(pAv, pA_vertex_count, pBv, pB_vertex_count) &&
		   impl::PolygonsHaveOverlapAxis(pBv, pB_vertex_count, pAv, pA_vertex_count);
}

} // namespace impl

bool Overlaps(const V2_float& point, const Transform& transform, Line line) {
	line.start *= transform.scale;
	line.end   *= transform.scale;
	line.start += transform.position;
	line.end   += transform.position;
	return impl::OverlapPointLine(point, line.start, line.end);
}

bool Overlaps(const V2_float& point, const Transform& transform, Circle circle) {
	circle.radius *= transform.scale.x;
	return impl::OverlapPointCircle(point, transform.position, circle.radius);
}

bool Overlaps(const V2_float& point, const Transform& transform, Triangle triangle) {
	for (auto& v : triangle.vertices) {
		v *= transform.scale;
		v += transform.position;
	}
	return impl::OverlapPointTriangle(
		point, triangle.vertices[0], triangle.vertices[1], triangle.vertices[2]
	);
}

bool Overlaps(const V2_float& point, Transform transform, Rect rect) {
	rect.size		   *= transform.scale;
	transform.position += rect.GetCenterOffset();
	return impl::OverlapPointRect(point, transform.position, rect.size, transform.rotation);
}

bool Overlaps(const V2_float& point, const Transform& transform, Capsule capsule) {
	capsule.start  *= transform.scale;
	capsule.end	   *= transform.scale;
	capsule.start  += transform.position;
	capsule.end	   += transform.position;
	capsule.radius *= transform.scale.x;
	return impl::OverlapPointCapsule(point, capsule.start, capsule.end, capsule.radius);
}

bool Overlaps(const Transform& a, Line A, const Transform& b, Line B) {
	A.start *= a.scale;
	A.end	*= a.scale;
	A.start += a.position;
	A.end	+= a.position;
	B.start *= b.scale;
	B.end	*= b.scale;
	B.start += b.position;
	B.end	+= b.position;
	return impl::OverlapLineLine(A.start, A.end, B.start, B.end);
}

bool Overlaps(const Transform& a, Line A, const Transform& b, Circle B) {
	A.start	 *= a.scale;
	A.end	 *= a.scale;
	A.start	 += a.position;
	A.end	 += a.position;
	B.radius *= b.scale.x;
	return impl::OverlapLineCircle(A.start, A.end, b.position, B.radius);
}

bool Overlaps(const Transform& a, Line A, const Transform& b, Triangle B) {
	A.start *= a.scale;
	A.end	*= a.scale;
	A.start += a.position;
	A.end	+= a.position;
	for (auto& v : B.vertices) {
		v *= b.scale;
		v += b.position;
	}
	return impl::OverlapLineTriangle(A.start, A.end, B.vertices[0], B.vertices[1], B.vertices[2]);
}

bool Overlaps(const Transform& a, Line A, Transform b, Rect B) {
	A.start	   *= a.scale;
	A.end	   *= a.scale;
	A.start	   += a.position;
	A.end	   += a.position;
	B.size	   *= b.scale;
	b.position += B.GetCenterOffset();
	return impl::OverlapLineRect(A.start, A.end, b.position, B.size);
}

bool Overlaps(const Transform& a, Line A, const Transform& b, Polygon B) {
	A.start *= a.scale;
	A.end	*= a.scale;
	A.start += a.position;
	A.end	+= a.position;
	for (auto& v : B.vertices) {
		v *= b.scale;
		v += b.position;
	}
	return impl::OverlapLinePolygon(A.start, A.end, B.vertices.data(), B.vertices.size());
}

bool Overlaps(const Transform& a, const Circle& A, const Transform& b, const Line& B) {
	return Overlaps(b, B, a, A);
}

bool Overlaps(const Transform& a, Circle A, const Transform& b, Circle B) {
	A.radius *= a.scale.x;
	B.radius *= b.scale.x;
	return impl::OverlapCircleCircle(a.position, A.radius, b.position, B.radius);
}

bool Overlaps(const Transform& a, Circle A, const Transform& b, Triangle B) {
	A.radius *= a.scale.x;
	for (auto& v : B.vertices) {
		v *= b.scale;
		v += b.position;
	}
	return impl::OverlapCircleTriangle(
		a.position, A.radius, B.vertices[0], B.vertices[1], B.vertices[2]
	);
}

bool Overlaps(const Transform& a, Circle A, Transform b, Rect B) {
	A.radius   *= a.scale.x;
	B.size	   *= b.scale;
	b.position += B.GetCenterOffset();
	return impl::OverlapCircleRect(a.position, A.radius, b.position, B.size);
}

bool Overlaps(const Transform& a, Circle A, const Transform& b, Polygon B) {
	A.radius *= a.scale.x;
	for (auto& v : B.vertices) {
		v *= b.scale;
		v += b.position;
	}
	return impl::OverlapCirclePolygon(a.position, A.radius, B.vertices.data(), B.vertices.size());
}

bool Overlaps(const Transform& a, const Triangle& A, const Transform& b, const Line& B) {
	return Overlaps(b, B, a, A);
}

bool Overlaps(const Transform& a, const Triangle& A, const Transform& b, const Circle& B) {
	return Overlaps(b, B, a, A);
}

bool Overlaps(const Transform& a, Triangle A, const Transform& b, Triangle B) {
	for (auto& v : A.vertices) {
		v *= a.scale;
		v += a.position;
	}
	for (auto& v : B.vertices) {
		v *= b.scale;
		v += b.position;
	}
	return impl::OverlapTriangleTriangle(
		A.vertices[0], A.vertices[1], A.vertices[2], B.vertices[0], B.vertices[1], B.vertices[2]
	);
}

bool Overlaps(const Transform& a, Triangle A, Transform b, Rect B) {
	for (auto& v : A.vertices) {
		v *= a.scale;
		v += a.position;
	}
	B.size	   *= b.scale;
	b.position += B.GetCenterOffset();
	return impl::OverlapTriangleRect(
		A.vertices[0], A.vertices[1], A.vertices[2], b.position, B.size, b.rotation
	);
}

bool Overlaps(const Transform& a, Triangle A, const Transform& b, Polygon B) {
	for (auto& v : A.vertices) {
		v *= a.scale;
		v += a.position;
	}
	for (auto& v : B.vertices) {
		v *= b.scale;
		v += b.position;
	}
	return impl::OverlapTrianglePolygon(
		A.vertices[0], A.vertices[1], A.vertices[2], B.vertices.data(), B.vertices.size()
	);
}

bool Overlaps(Transform a, const Rect& A, const Transform& b, const Line& B) {
	return Overlaps(b, B, a, A);
}

bool Overlaps(Transform a, const Rect& A, const Transform& b, const Circle& B) {
	return Overlaps(b, B, a, A);
}

bool Overlaps(Transform a, const Rect& A, const Transform& b, const Triangle& B) {
	return Overlaps(b, B, a, A);
}

bool Overlaps(Transform a, Rect A, Transform b, Rect B) {
	A.size	   *= a.scale;
	a.position += A.GetCenterOffset();
	B.size	   *= b.scale;
	b.position += B.GetCenterOffset();
	return impl::OverlapRectRect(a.position, A.size, a.rotation, b.position, B.size, b.rotation);
}

bool Overlaps(Transform a, Rect A, const Transform& b, Polygon B) {
	A.size	   *= a.scale;
	a.position += A.GetCenterOffset();
	for (auto& v : B.vertices) {
		v *= b.scale;
		v += b.position;
	}
	return impl::OverlapRectPolygon(
		a.position, A.size, a.rotation, B.vertices.data(), B.vertices.size()
	);
}

bool Overlaps(const Transform& a, const Polygon& A, const Transform& b, Line B) {
	return Overlaps(b, B, a, A);
}

bool Overlaps(const Transform& a, const Polygon& A, const Transform& b, Circle B) {
	return Overlaps(b, B, a, A);
}

bool Overlaps(const Transform& a, const Polygon& A, const Transform& b, Triangle B) {
	return Overlaps(b, B, a, A);
}

bool Overlaps(const Transform& a, const Polygon& A, const Transform& b, Rect B) {
	return Overlaps(b, B, a, A);
}

bool Overlaps(const Transform& a, Polygon A, const Transform& b, Polygon B) {
	for (auto& v : A.vertices) {
		v *= a.scale;
		v += a.position;
	}
	for (auto& v : B.vertices) {
		v *= b.scale;
		v += b.position;
	}
	return impl::OverlapPolygonPolygon(
		A.vertices.data(), A.vertices.size(), B.vertices.data(), B.vertices.size()
	);
}

} // namespace ptgn