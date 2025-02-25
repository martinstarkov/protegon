#include "math/collision/overlap.h"

#include <limits>
#include <utility>
#include <vector>

#include "core/game.h"
#include "math/geometry/axis.h"
#include "math/geometry/polygon.h"
#include "math/math.h"
#include "math/utility.h"
#include "math/vector2.h"
#include "utility/assert.h"
#include "utility/debug.h"
#include "utility/utility.h"

namespace ptgn {

namespace impl {

bool PolygonsHaveOverlapAxis(
	const std::vector<V2_float>& polygonA, const std::vector<V2_float>& polygonB
) {
	const auto axes{ impl::GetPolygonAxes(polygonA, false) };
	for (const Axis& a : axes) {
		auto [min1, max1] = impl::GetPolygonProjectionMinMax(polygonA, a);
		auto [min2, max2] = impl::GetPolygonProjectionMinMax(polygonB, a);

		if (!impl::IntervalsOverlap(min1, max1, min2, max2)) {
			return false;
		}
	}
	return true;
}

bool PolygonContainsPolygon(
	const std::vector<V2_float>& polygonA, const std::vector<V2_float>& polygonB
) {
	for (const auto& p : polygonB) {
		if (!OverlapPointPolygon(p, polygonA)) {
			return false;
		}
	}
	return true;
}

bool GetPolygonMinimumOverlap(
	const std::vector<V2_float>& pA, const std::vector<V2_float>& pB, float& depth, Axis& axis
) {
	const auto axes{ impl::GetPolygonAxes(pA, true) };
	for (const Axis& a : axes) {
		auto [min1, max1] = impl::GetPolygonProjectionMinMax(pA, a);
		auto [min2, max2] = impl::GetPolygonProjectionMinMax(pB, a);

		if (!impl::IntervalsOverlap(min1, max1, min2, max2)) {
			return false;
		}
		bool contained{ PolygonContainsPolygon(pA, pB) || PolygonContainsPolygon(pB, pA) };

		float o{ impl::GetIntervalOverlap(min1, max1, min2, max2, contained, axis.direction) };

		if (o < depth) {
			depth = o;
			axis  = a;
		}
	}
	return true;
}

} // namespace impl

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
	const V2_float& point, const V2_float& rect_min, const V2_float& rect_max, float rotation,
	const V2_float& rotation_center
) {
#ifdef PTGN_DEBUG
	game.stats.overlap_point_rect++;
#endif
	if (rotation != 0.0f) {
		auto polygon{ impl::GetQuadVertices(rect_min, rect_max, rotation, rotation_center) };
		return OverlapPointPolygon(point, ToVector(polygon));
	}

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

bool OverlapPointPolygon(const V2_float& point, const std::vector<V2_float>& v) {
#ifdef PTGN_DEBUG
	game.stats.overlap_point_polygon++;
#endif
	std::size_t count{ v.size() };
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

bool OverlapCircleRect(
	const V2_float& circle_center, float circle_radius, const V2_float& rect_min,
	const V2_float& rect_max
) {
#ifdef PTGN_DEBUG
	game.stats.overlap_circle_rect++;
#endif
	// Source:
	// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 165-166.
	// TODO: Add rotation check.
	return impl::WithinPerimeter(
		circle_radius, impl::SquareDistancePointRect(circle_center, rect_min, rect_max)
	);
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

bool OverlapLineRect(
	const V2_float& line_start, const V2_float& line_end, const V2_float& rect_min,
	const V2_float& rect_max
) {
#ifdef PTGN_DEBUG
	game.stats.overlap_line_rect++;
#endif
	// TODO: Add rotation check.

	V2_float c{ Midpoint(rect_min, rect_max) };
	V2_float e{ (rect_max - rect_min) / 2.0f };
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

bool OverlapTriangleRect(
	const V2_float& triangle_a, const V2_float& triangle_b, const V2_float& triangle_c,
	const V2_float& rect_min, const V2_float& rect_max, float rotation,
	const V2_float& rotation_center
) {
#ifdef PTGN_DEBUG
	game.stats.overlap_triangle_rect++;
#endif
	auto polygon{ impl::GetQuadVertices(rect_min, rect_max, rotation, rotation_center) };
	return OverlapPolygonPolygon({ triangle_a, triangle_b, triangle_c }, ToVector(polygon));
}

bool OverlapRectRect(
	const V2_float& rectA_min, const V2_float& rectA_max, float rotationA,
	const V2_float& rotation_centerA, const V2_float& rectB_min, const V2_float& rectB_max,
	float rotationB, const V2_float& rotation_centerB
) {
	if (rotationA != 0.0f || rotationB != 0.0f) {
		auto polygonA{ impl::GetQuadVertices(rectA_min, rectA_max, rotationA, rotation_centerA) };
		auto polygonB{ impl::GetQuadVertices(rectB_min, rectB_max, rotationB, rotation_centerB) };
		return OverlapPolygonPolygon(ToVector(polygonA), ToVector(polygonB));
	}
#ifdef PTGN_DEBUG
	game.stats.overlap_rect_rect++;
#endif

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
	const V2_float& rect_min, const V2_float& rect_max, float rotation,
	const V2_float& rotation_center, const V2_float& capsule_start, const V2_float& capsule_end,
	float capsule_radius
) {
#ifdef PTGN_DEBUG
	game.stats.overlap_rect_capsule++;
#endif
	if (OverlapPointRect(capsule_start, rect_min, rect_max, rotation, rotation_center)) {
		return true;
	}
	if (OverlapPointRect(capsule_end, rect_min, rect_max, rotation, rotation_center)) {
		return true;
	}

	// Rotated rect.
	if (rotation != 0.0f) {
		auto polygon{ impl::GetQuadVertices(rect_min, rect_max, rotation, rotation_center) };
		PTGN_ASSERT(polygon.size() == 4);
		if (OverlapLineCapsule(
				polygon[0], polygon[1], capsule_start, capsule_end, capsule_radius
			)) {
			return true;
		}
		if (OverlapLineCapsule(
				polygon[1], polygon[2], capsule_start, capsule_end, capsule_radius
			)) {
			return true;
		}
		if (OverlapLineCapsule(
				polygon[2], polygon[3], capsule_start, capsule_end, capsule_radius
			)) {
			return true;
		}
		if (OverlapLineCapsule(
				polygon[3], polygon[0], capsule_start, capsule_end, capsule_radius
			)) {
			return true;
		}
		return false;
	}
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
	const V2_float& rect_min, const V2_float& rect_max, float rotation,
	const V2_float& rotation_center, const std::vector<V2_float>& polygon
) {
	auto polygonB{ impl::GetQuadVertices(rect_min, rect_max, rotation, rotation_center) };
	return OverlapPolygonPolygon(ToVector(polygonB), polygon);
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

bool OverlapPolygonPolygon(const std::vector<V2_float>& pA, const std::vector<V2_float>& pB) {
#ifdef PTGN_DEBUG
	game.stats.overlap_polygon_polygon++;
#endif
	PTGN_ASSERT(
		impl::IsConvexPolygon(pA) && impl::IsConvexPolygon(pB),
		"PolygonPolygon overlap check only works if both polygons are convex"
	);
	return impl::PolygonsHaveOverlapAxis(pA, pB) && impl::PolygonsHaveOverlapAxis(pB, pA);
}

} // namespace ptgn