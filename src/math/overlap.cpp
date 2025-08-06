#include "math/overlap.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>
#include <variant>
#include <vector>

#include "common/assert.h"
#include "components/transform.h"
#include "core/game.h"
#include "debug/debugging.h"
#include "debug/log.h"
#include "debug/stats.h"
#include "geometry/capsule.h"
#include "geometry/circle.h"
#include "geometry/line.h"
#include "geometry/polygon.h"
#include "geometry/rect.h"
#include "geometry/triangle.h"
#include "math/geometry.h"
#include "math/geometry/axis.h"
#include "math/utility.h"
#include "math/vector2.h"

#define PTGN_HANDLE_OVERLAP_SOLO_PAIR(TypeA, TypeB, PREFIX)                 \
	if constexpr (std::is_same_v<S1, TypeA> && std::is_same_v<S2, TypeB>) { \
		return impl::PREFIX##TypeA##TypeB(t1, s1, t2, s2);                  \
	} else

#define PTGN_HANDLE_OVERLAP_PAIR(TypeA, TypeB, PREFIX)                             \
	if constexpr (std::is_same_v<S1, TypeA> && std::is_same_v<S2, TypeB>) {        \
		return impl::PREFIX##TypeA##TypeB(t1, s1, t2, s2);                         \
	} else if constexpr (std::is_same_v<S1, TypeB> && std::is_same_v<S2, TypeA>) { \
		return impl::PREFIX##TypeA##TypeB(t2, s2, t1, s1);                         \
	} else

#define PTGN_OVERLAP_SHAPE_PAIR_TABLE                          \
	PTGN_HANDLE_OVERLAP_SOLO_PAIR(Point, Point, Overlap)       \
	PTGN_HANDLE_OVERLAP_PAIR(Point, Line, Overlap)             \
	PTGN_HANDLE_OVERLAP_PAIR(Point, Triangle, Overlap)         \
	PTGN_HANDLE_OVERLAP_PAIR(Point, Capsule, Overlap)          \
	PTGN_HANDLE_OVERLAP_PAIR(Point, Rect, Overlap)             \
	PTGN_HANDLE_OVERLAP_PAIR(Point, Circle, Overlap)           \
	PTGN_HANDLE_OVERLAP_PAIR(Point, Polygon, Overlap)          \
	PTGN_HANDLE_OVERLAP_PAIR(Line, Line, Overlap)              \
	PTGN_HANDLE_OVERLAP_PAIR(Line, Triangle, Overlap)          \
	PTGN_HANDLE_OVERLAP_PAIR(Line, Capsule, Overlap)           \
	PTGN_HANDLE_OVERLAP_PAIR(Line, Rect, Overlap)              \
	PTGN_HANDLE_OVERLAP_PAIR(Line, Circle, Overlap)            \
	PTGN_HANDLE_OVERLAP_PAIR(Line, Polygon, Overlap)           \
	PTGN_HANDLE_OVERLAP_SOLO_PAIR(Circle, Circle, Overlap)     \
	PTGN_HANDLE_OVERLAP_PAIR(Circle, Rect, Overlap)            \
	PTGN_HANDLE_OVERLAP_PAIR(Circle, Polygon, Overlap)         \
	PTGN_HANDLE_OVERLAP_PAIR(Circle, Triangle, Overlap)        \
	PTGN_HANDLE_OVERLAP_PAIR(Circle, Capsule, Overlap)         \
	PTGN_HANDLE_OVERLAP_SOLO_PAIR(Triangle, Triangle, Overlap) \
	PTGN_HANDLE_OVERLAP_PAIR(Triangle, Capsule, Overlap)       \
	PTGN_HANDLE_OVERLAP_PAIR(Triangle, Rect, Overlap)          \
	PTGN_HANDLE_OVERLAP_PAIR(Triangle, Polygon, Overlap)       \
	PTGN_HANDLE_OVERLAP_SOLO_PAIR(Rect, Rect, Overlap)         \
	PTGN_HANDLE_OVERLAP_PAIR(Rect, Polygon, Overlap)           \
	PTGN_HANDLE_OVERLAP_PAIR(Rect, Capsule, Overlap)           \
	PTGN_HANDLE_OVERLAP_SOLO_PAIR(Polygon, Polygon, Overlap)   \
	PTGN_HANDLE_OVERLAP_PAIR(Polygon, Capsule, Overlap)        \
	PTGN_HANDLE_OVERLAP_SOLO_PAIR(Capsule, Capsule, Overlap)

namespace ptgn {

namespace impl {

bool PolygonsHaveOverlapAxis(
	const Transform& t1, const Polygon& A, const Transform& t2, const Polygon& B
) {
	auto world_pointsA{ A.GetWorldVertices(t1) };
	auto world_pointsB{ B.GetWorldVertices(t2) };

	const auto axes{ impl::GetPolygonAxes(world_pointsA.data(), world_pointsA.size(), false) };
	for (const Axis& a : axes) {
		auto [min1, max1] =
			impl::GetPolygonProjectionMinMax(world_pointsA.data(), world_pointsA.size(), a);
		auto [min2, max2] =
			impl::GetPolygonProjectionMinMax(world_pointsB.data(), world_pointsB.size(), a);

		if (!impl::IntervalsOverlap(min1, max1, min2, max2)) {
			return false;
		}
	}
	return true;
}

bool GetPolygonMinimumOverlap(
	const Transform& t1, const Polygon& A, const Transform& t2, const Polygon& B, float& depth,
	Axis& axis
) {
	Polygon world_polygonA{ A.GetWorldVertices(t1) };
	Polygon world_polygonB{ B.GetWorldVertices(t2) };

	const auto axes{
		impl::GetPolygonAxes(world_polygonA.vertices.data(), world_polygonA.vertices.size(), true)
	};
	for (const Axis& a : axes) {
		auto [min1, max1] = impl::GetPolygonProjectionMinMax(
			world_polygonA.vertices.data(), world_polygonA.vertices.size(), a
		);
		auto [min2, max2] = impl::GetPolygonProjectionMinMax(
			world_polygonB.vertices.data(), world_polygonB.vertices.size(), a
		);

		if (!impl::IntervalsOverlap(min1, max1, min2, max2)) {
			return false;
		}
		bool contained{
			PolygonContainsPolygon(Transform{}, world_polygonA, Transform{}, world_polygonB) ||
			PolygonContainsPolygon(Transform{}, world_polygonB, Transform{}, world_polygonA)
		};

		float o{ impl::GetIntervalOverlap(min1, max1, min2, max2, contained, axis.direction) };

		if (o < depth) {
			depth = o;
			axis  = a;
		}
	}
	return true;
}

bool LineContainsLine(const Transform& t1, const Line& A, const Transform& t2, const Line& B) {
	auto [lineA_start, lineA_end] = A.GetWorldVertices(t1);
	auto [lineB_start, lineB_end] = B.GetWorldVertices(t2);

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

	if (OverlapPointLine(Transform{}, lineB_start, Transform{}, Line{ lineA_start, lineA_end }) &&
		OverlapPointLine(Transform{}, lineB_end, Transform{}, Line{ lineA_start, lineA_end })) {
		return true;
	}

	return false;
}

bool PolygonContainsPolygon(
	const Transform& t1, const Polygon& A, const Transform& t2, const Polygon& B
) {
	Polygon world_polygonA{ A.GetWorldVertices(t1) };
	Polygon world_polygonB{ B.GetWorldVertices(t2) };

	for (const auto& vertexB : world_polygonB.vertices) {
		if (!OverlapPointPolygon(Transform{}, vertexB, Transform{}, world_polygonA)) {
			return false;
		}
	}
	return true;
}

bool TriangleContainsTriangle(
	const Transform& t1, const Triangle& A, const Transform& t2, const Triangle& B
) {
	auto [v1A, v2A, v3A] = A.GetWorldVertices(t1);
	auto [v1B, v2B, v3B] = B.GetWorldVertices(t2);
	return OverlapPointTriangle(Transform{}, v1B, Transform{}, Triangle{ v1A, v2A, v3A }) &&
		   OverlapPointTriangle(Transform{}, v2B, Transform{}, Triangle{ v1A, v2A, v3A }) &&
		   OverlapPointTriangle(Transform{}, v3B, Transform{}, Triangle{ v1A, v2A, v3A });
}

bool PolygonContainsTriangle(
	const Transform& t1, const Polygon& A, const Transform& t2, const Triangle& B
) {
	auto [a, b, c] = B.GetWorldVertices(t2);
	Polygon world_polygon{ A.GetWorldVertices(t1) };
	return OverlapPointPolygon(Transform{}, a, Transform{}, world_polygon) &&
		   OverlapPointPolygon(Transform{}, b, Transform{}, world_polygon) &&
		   OverlapPointPolygon(Transform{}, c, Transform{}, world_polygon);
}

bool OverlapPointPoint(const Transform& t1, const Point& A, const Transform& t2, const Point& B) {
	return ToWorldPoint(A, t1) == ToWorldPoint(B, t2);
}

bool OverlapPointLine(const Transform& t1, const Point& A, const Transform& t2, const Line& B) {
#ifdef PTGN_DEBUG
	game.stats.overlap_point_line++;
#endif
	auto point					= ToWorldPoint(A, t1);
	auto [line_start, line_end] = B.GetWorldVertices(t2);

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
	const Transform& t1, const Point& A, const Transform& t2, const Triangle& B
) {
#ifdef PTGN_DEBUG
	game.stats.overlap_point_triangle++;
#endif
	auto point								  = ToWorldPoint(A, t1);
	auto [triangle_a, triangle_b, triangle_c] = B.GetWorldVertices(t2);

	// Using barycentric coordinates method.
	float a{ 0.5f * (-triangle_b.y * triangle_c.x + triangle_a.y * (-triangle_b.x + triangle_c.x) +
					 triangle_a.x * (triangle_b.y - triangle_c.y) + triangle_b.x * triangle_c.y) };
	float z{ 1.0f / (2.0f * a) };
	float s{ z *
			 (triangle_a.y * triangle_c.x - triangle_a.x * triangle_c.y +
			  (triangle_c.y - triangle_a.y) * point.x + (triangle_a.x - triangle_c.x) * point.y) };
	float t{ z *
			 (triangle_a.x * triangle_b.y - triangle_a.y * triangle_b.x +
			  (triangle_a.y - triangle_b.y) * point.x + (triangle_b.x - triangle_a.x) * point.y) };

	return s >= 0.0f && t >= 0.0f && (s + t) <= 1.0f;
}

bool OverlapPointCircle(const Transform& t1, const Point& A, const Transform& t2, const Circle& B) {
	auto circle_radius{ B.GetRadius(t2) };
	if (circle_radius <= 0.0f) {
		return false;
	}
#ifdef PTGN_DEBUG
	game.stats.overlap_point_circle++;
#endif
	auto point = ToWorldPoint(A, t1);
	auto circle_center{ B.GetCenter(t2) };

	V2_float dist{ circle_center - point };
	return impl::WithinPerimeter(circle_radius, dist.Dot(dist));
}

bool OverlapPointRect(const Transform& t1, const Point& A, const Transform& t2, const Rect& B) {
	auto rect_size{ B.GetSize(t2) };
	if (rect_size.IsZero()) {
		return false;
	}
#ifdef PTGN_DEBUG
	game.stats.overlap_point_rect++;
#endif
	if (t2.rotation != 0.0f) {
		return OverlapPointPolygon(t1, A, t2, Polygon{ B.GetLocalVertices() });
	}

	auto point = ToWorldPoint(A, t1);
	auto rect_center{ B.GetCenter(t2) };

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
	const Transform& t1, const Point& A, const Transform& t2, const Capsule& B
) {
	auto capsule_radius{ B.GetRadius(t2) };
	if (capsule_radius <= 0.0f) {
		return false;
	}
#ifdef PTGN_DEBUG
	game.stats.overlap_point_capsule++;
#endif

	auto point						  = ToWorldPoint(A, t1);
	auto [capsule_start, capsule_end] = B.GetWorldVertices(t2);

	// Source:
	// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 114.
	return impl::WithinPerimeter(
		capsule_radius, impl::SquareDistancePointLine(point, capsule_start, capsule_end)
	);
}

bool OverlapPointPolygon(
	const Transform& t1, const Point& A, const Transform& t2, const Polygon& B
) {
#ifdef PTGN_DEBUG
	game.stats.overlap_point_polygon++;
#endif
	auto point = ToWorldPoint(A, t1);

	auto world_points{ ToWorldPoint(B.vertices, t2) };
	std::size_t count{ world_points.size() };
	const auto& v{ world_points };

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

bool OverlapLineLine(const Transform& t1, const Line& A, const Transform& t2, const Line& B) {
#ifdef PTGN_DEBUG
	game.stats.overlap_line_line++;
#endif
	auto [lineA_start, lineA_end] = A.GetWorldVertices(t1);
	auto [lineB_start, lineB_end] = B.GetWorldVertices(t2);
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

bool OverlapLineCircle(const Transform& t1, const Line& A, const Transform& t2, const Circle& B) {
	auto circle_radius{ B.GetRadius(t2) };
	if (circle_radius <= 0.0f) {
		return false;
	}
#ifdef PTGN_DEBUG
	game.stats.overlap_line_circle++;
#endif
	auto [line_start, line_end] = A.GetWorldVertices(t1);
	// Source: https://www.baeldung.com/cs/circle-line-segment-collision-detection

	// If the line is inside the circle entirely, exit early.
	if (OverlapPointCircle(Transform{}, line_start, t2, B) &&
		OverlapPointCircle(Transform{}, line_end, t2, B)) {
		return true;
	}

	float min_dist2{ std::numeric_limits<float>::infinity() };

	auto circle_center{ B.GetCenter(t2) };

	// O is the circle center, P is the line start, Q is the line end.
	V2_float OP{ line_start - circle_center };
	V2_float OQ{ line_end - circle_center };
	V2_float PQ{ line_end - line_start };

	float OP_dist2{ OP.Dot(OP) };
	float OQ_dist2{ OQ.Dot(OQ) };
	float max_dist2{ std::max(OP_dist2, OQ_dist2) };

	if (OP.Dot(-PQ) > 0.0f && OQ.Dot(PQ) > 0.0f) {
		float triangle_area{ Abs(impl::ParallelogramArea(circle_center, line_start, line_end)) /
							 2.0f };
		min_dist2 = 4.0f * triangle_area * triangle_area / PQ.Dot(PQ);
	} else {
		min_dist2 = std::min(OP_dist2, OQ_dist2);
	}

	return impl::WithinPerimeter(circle_radius, min_dist2) &&
		   !impl::WithinPerimeter(circle_radius, max_dist2);
}

bool OverlapLineTriangle(
	const Transform& t1, const Line& A, const Transform& t2, const Triangle& B
) {
	auto [line_start, line_end] = A.GetWorldVertices(t1);
	auto [a, b, c]				= B.GetWorldVertices(t2);
	return OverlapPointTriangle(Transform{}, line_start, Transform{}, Triangle{ a, b, c }) ||
		   OverlapPointTriangle(Transform{}, line_end, Transform{}, Triangle{ a, b, c }) ||
		   OverlapLineLine(Transform{}, { line_start, line_end }, Transform{}, Line{ a, b }) ||
		   OverlapLineLine(Transform{}, { line_start, line_end }, Transform{}, Line{ b, c }) ||
		   OverlapLineLine(Transform{}, { line_start, line_end }, Transform{}, Line{ c, a });
}

bool OverlapLineRect(const Transform& t1, const Line& A, const Transform& t2, const Rect& B) {
	auto rect_size{ B.GetSize(t2) };
	if (rect_size.IsZero()) {
		return false;
	}
	auto [line_start, line_end] = A.GetWorldVertices(t1);
	if (t2.rotation != 0.0f) {
		return OverlapLinePolygon(
			Transform{}, Line{ line_start, line_end }, t2, Polygon{ B.GetLocalVertices() }
		);
	}

#ifdef PTGN_DEBUG
	game.stats.overlap_line_rect++;
#endif
	auto rect_center{ B.GetCenter(t2) };

	V2_float c{ rect_center };
	V2_float e{ rect_size * 0.5f };
	V2_float m{ Midpoint(line_start, line_end) };
	V2_float d{ line_end - m }; // Line halflength vector

	m = m - c;					// Translate box and segment to origin

	// Try world coordinate axes as separating axes.
	float adx{ Abs(d.x) };
	if (Abs(m.x) >= e.x + adx) {
		return false;
	}

	float ady{ Abs(d.y) };
	if (Abs(m.y) >= e.y + ady) {
		return false;
	}

	// Add in an epsilon term to counteract arithmetic errors when segment is
	// (near) parallel to a coordinate axis.
	adx += epsilon<float>;
	ady += epsilon<float>;

	// Try cross products of segment direction vector with coordinate axes.
	float cross{ m.Cross(d) };

	if (float dot{ e.Dot({ ady, adx }) }; Abs(cross) > dot) {
		return false;
	}

	// No separating axis found; segment must be overlapping AABB.
	return true;

	// Alternative method:
	// Source: https://en.wikipedia.org/wiki/Cohen%E2%80%93Sutherland_algorithm
}

bool OverlapLineCapsule(const Transform& t1, const Line& A, const Transform& t2, const Capsule& B) {
	auto capsule_radius{ B.GetRadius(t2) };
	if (capsule_radius <= 0.0f) {
		return false;
	}
#ifdef PTGN_DEBUG
	game.stats.overlap_line_capsule++;
#endif
	auto [line_start, line_end]		  = A.GetWorldVertices(t1);
	auto [capsule_start, capsule_end] = B.GetWorldVertices(t2);
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

bool OverlapLinePolygon(const Transform& t1, const Line& A, const Transform& t2, const Polygon& B) {
	auto [line_start, line_end] = A.GetWorldVertices(t1);
	auto polygon_vertices		= B.GetWorldVertices(t2);
	if (OverlapPointPolygon(Transform{}, line_start, Transform{}, Polygon{ polygon_vertices })) {
		return true;
	}

	std::size_t polygon_vertex_count{ polygon_vertices.size() };

	PTGN_ASSERT(impl::IsConvexPolygon(polygon_vertices.data(), polygon_vertex_count));

	for (std::size_t i{ 0 }; i < polygon_vertex_count; ++i) {
		if (OverlapLineLine(
				Transform{}, { line_start, line_end }, Transform{},
				Line{ polygon_vertices[i], polygon_vertices[(i + 1) % polygon_vertex_count] }
			)) {
			return true;
		}
	}
	return false;
}

bool OverlapCircleCircle(
	const Transform& t1, const Circle& A, const Transform& t2, const Circle& B
) {
#ifdef PTGN_DEBUG
	game.stats.overlap_circle_circle++;
#endif
	auto circleA_center{ A.GetCenter(t1) };
	auto circleB_center{ B.GetCenter(t2) };
	auto circleA_radius{ A.GetRadius(t1) };
	auto circleB_radius{ B.GetRadius(t2) };
	// Source:
	// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 88.
	V2_float dist{ circleA_center - circleB_center };

	return impl::WithinPerimeter(circleA_radius + circleB_radius, dist.Dot(dist));
}

bool OverlapCircleTriangle(
	const Transform& t1, const Circle& A, const Transform& t2, const Triangle& B
) {
	if (auto circle_radius{ A.GetRadius(t1) }; circle_radius <= 0.0f) {
		return false;
	}
	auto circle_center{ A.GetCenter(t1) };
	auto [a, b, c] = B.GetWorldVertices(t2);
	return OverlapPointTriangle(Transform{}, circle_center, Transform{}, Triangle{ a, b, c }) ||
		   OverlapLineCircle(Transform{}, Line{ a, b }, t1, A) ||
		   OverlapLineCircle(Transform{}, Line{ b, c }, t1, A) ||
		   OverlapLineCircle(Transform{}, Line{ c, b }, t1, A);
}

bool OverlapCircleRect(const Transform& t1, const Circle& A, const Transform& t2, const Rect& B) {
	auto circle_radius{ A.GetRadius(t1) };
	if (circle_radius <= 0.0f) {
		return false;
	}
	auto rect_size{ B.GetSize(t2) };
	if (rect_size.IsZero()) {
		return false;
	}
	if (t2.rotation != 0.0f) {
		return OverlapCirclePolygon(t1, A, t2, Polygon{ B.GetLocalVertices() });
	}
#ifdef PTGN_DEBUG
	game.stats.overlap_circle_rect++;
#endif
	auto circle_center{ A.GetCenter(t1) };
	auto rect_center{ B.GetCenter(t2) };
	// Source:
	// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 165-166.
	auto half{ rect_size * 0.5f };
	auto rect_min{ rect_center - half };
	auto rect_max{ rect_center + half };

	return impl::WithinPerimeter(
		circle_radius, impl::SquareDistancePointRect(circle_center, rect_min, rect_max)
	);
}

bool OverlapCirclePolygon(
	const Transform& t1, const Circle& A, const Transform& t2, const Polygon& B
) {
	if (auto circle_radius{ A.GetRadius(t1) }; circle_radius <= 0.0f) {
		return false;
	}

	if (auto circle_center{ A.GetCenter(t1) };
		OverlapPointPolygon(Transform{}, circle_center, t2, B)) {
		return true;
	}

	auto polygon_vertices{ B.GetWorldVertices(t2) };
	std::size_t polygon_vertex_count{ polygon_vertices.size() };

	PTGN_ASSERT(impl::IsConvexPolygon(polygon_vertices.data(), polygon_vertex_count));

	for (std::size_t i{ 0 }; i < polygon_vertex_count; ++i) {
		if (OverlapLineCircle(
				Transform{},
				Line{ polygon_vertices[i], polygon_vertices[(i + 1) % polygon_vertex_count] }, t1, A
			)) {
			return true;
		}
	}

	return false;
}

bool OverlapCircleCapsule(
	const Transform& t1, const Circle& A, const Transform& t2, const Capsule& B
) {
	auto circle_radius{ A.GetRadius(t1) };
	if (circle_radius <= 0.0f) {
		return false;
	}
	auto capsule_radius{ B.GetRadius(t2) };
	if (capsule_radius <= 0.0f) {
		return false;
	}
#ifdef PTGN_DEBUG
	game.stats.overlap_circle_capsule++;
#endif
	auto circle_center{ A.GetCenter(t1) };
	auto [capsule_start, capsule_end] = B.GetWorldVertices(t2);
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
	const Transform& t1, const Triangle& A, const Transform& t2, const Triangle& B
) {
	auto [A_a, A_b, A_c] = A.GetWorldVertices(t1);
	auto [B_a, B_b, B_c] = B.GetWorldVertices(t2);
	return OverlapPointTriangle(Transform{}, A_a, Transform{}, Triangle{ B_a, B_b, B_c }) ||
		   OverlapLineTriangle(
			   Transform{}, Line{ B_a, B_b }, Transform{}, Triangle{ A_a, A_b, A_c }
		   ) ||
		   OverlapLineTriangle(
			   Transform{}, Line{ B_b, B_c }, Transform{}, Triangle{ A_a, A_b, A_c }
		   ) ||
		   OverlapLineTriangle(
			   Transform{}, Line{ B_c, B_a }, Transform{}, Triangle{ A_a, A_b, A_c }
		   );
}

bool OverlapTriangleRect(
	const Transform& t1, const Triangle& A, const Transform& t2, const Rect& B

) {
	if (auto rect_size{ B.GetSize(t2) }; rect_size.IsZero()) {
		return false;
	}
#ifdef PTGN_DEBUG
	game.stats.overlap_triangle_rect++;
#endif
	return OverlapPolygonPolygon(
		t1, Polygon{ A.GetLocalVertices() }, t2, Polygon{ B.GetLocalVertices() }
	);
}

bool OverlapTrianglePolygon(
	const Transform& t1, const Triangle& A, const Transform& t2, const Polygon& B
) {
	auto [a, b, c] = A.GetWorldVertices(t1);
	if (OverlapPointPolygon(Transform{}, a, t2, B)) {
		return true;
	}

	auto polygon_vertices{ B.GetWorldVertices(t2) };
	std::size_t polygon_vertex_count{ polygon_vertices.size() };

	PTGN_ASSERT(impl::IsConvexPolygon(polygon_vertices.data(), polygon_vertex_count));

	for (std::size_t i{ 0 }; i < polygon_vertex_count; ++i) {
		if (OverlapLineTriangle(
				Transform{},
				Line{ polygon_vertices[i], polygon_vertices[(i + 1) % polygon_vertex_count] },
				Transform{}, Triangle{ a, b, c }
			)) {
			return true;
		}
	}

	return false;
}

bool OverlapTriangleCapsule(
	const Transform& t1, const Triangle& A, const Transform& t2, const Capsule& B
) {
	auto capsule_radius{ B.GetRadius(t2) };
	if (capsule_radius <= 0.0f) {
		return false;
	}

#ifdef PTGN_DEBUG
	game.stats.overlap_triangle_capsule++;
#endif

	auto [capsule_start, capsule_end] = B.GetWorldVertices(t2);

	auto triangle_polygon{ A.GetWorldVertices(t1) };

	if (OverlapPointTriangle(
			Transform{}, capsule_start, Transform{}, Triangle{ triangle_polygon }
		)) {
		return true;
	}

	if (OverlapPointTriangle(Transform{}, capsule_end, Transform{}, Triangle{ triangle_polygon })) {
		return true;
	}

	return OverlapPolygonCapsule(
		Transform{}, Polygon{ triangle_polygon }, Transform{},
		Capsule{ capsule_start, capsule_end, capsule_radius }
	);
}

bool OverlapRectRect(const Transform& t1, const Rect& A, const Transform& t2, const Rect& B) {
	if (t1.rotation != 0.0f || t2.rotation != 0.0f) {
		return OverlapPolygonPolygon(
			t1, Polygon{ A.GetLocalVertices() }, t2, Polygon{ B.GetLocalVertices() }
		);
	}
#ifdef PTGN_DEBUG
	game.stats.overlap_rect_rect++;
#endif
	auto rectA_size{ A.GetSize(t1) };
	auto rectB_size{ B.GetSize(t2) };
	auto rectA_center{ A.GetCenter(t1) };
	auto rectB_center{ B.GetCenter(t2) };

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

bool OverlapRectCapsule(const Transform& t1, const Rect& A, const Transform& t2, const Capsule& B) {
	auto capsule_radius{ B.GetRadius(t2) };
	if (capsule_radius <= 0.0f) {
		return false;
	}

	auto rect_size{ A.GetSize(t1) };
	if (rect_size.IsZero()) {
		return false;
	}

#ifdef PTGN_DEBUG
	game.stats.overlap_rect_capsule++;
#endif
	auto [capsule_start, capsule_end] = B.GetWorldVertices(t2);

	if (OverlapPointRect(Transform{}, capsule_start, t1, A)) {
		return true;
	}

	if (OverlapPointRect(Transform{}, capsule_end, t1, A)) {
		return true;
	}

	// Rotated rect.
	if (t1.rotation != 0.0f) {
		return OverlapPolygonCapsule(
			t1, Polygon{ A.GetLocalVertices() }, Transform{},
			Capsule{ capsule_start, capsule_end, capsule_radius }
		);
	}

	auto rect_center{ A.GetCenter(t1) };
	auto half{ rect_size * 0.5f };
	auto rect_min{ rect_center - half };
	auto rect_max{ rect_center + half };

	// No rotation.
	if (OverlapLineCapsule(
			Transform{}, Line{ rect_min, { rect_max.x, rect_min.y } }, Transform{},
			Capsule{ capsule_start, capsule_end, capsule_radius }
		)) {
		return true;
	}

	if (OverlapLineCapsule(
			Transform{}, Line{ { rect_max.x, rect_min.y }, rect_max }, Transform{},
			Capsule{ capsule_start, capsule_end, capsule_radius }
		)) {
		return true;
	}

	if (OverlapLineCapsule(
			Transform{}, Line{ rect_max, { rect_min.x, rect_max.y } }, Transform{},
			Capsule{ capsule_start, capsule_end, capsule_radius }
		)) {
		return true;
	}

	if (OverlapLineCapsule(
			Transform{}, Line{ { rect_min.x, rect_max.y }, rect_min }, Transform{},
			Capsule{ capsule_start, capsule_end, capsule_radius }
		)) {
		return true;
	}

	return false;
}

bool OverlapRectPolygon(const Transform& t1, const Rect& A, const Transform& t2, const Polygon& B) {
	if (auto rect_size{ A.GetSize(t1) }; rect_size.IsZero()) {
		return false;
	}
	return OverlapPolygonPolygon(t1, Polygon{ A.GetLocalVertices() }, t2, B);
}

bool OverlapCapsuleCapsule(
	const Transform& t1, const Capsule& A, const Transform& t2, const Capsule& B
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

	auto capsuleA_radius{ A.GetRadius(t1) };
	auto capsuleB_radius{ B.GetRadius(t2) };

	auto [capsuleA_start, capsuleA_end] = A.GetWorldVertices(t1);
	auto [capsuleB_start, capsuleB_end] = B.GetWorldVertices(t2);

	return impl::WithinPerimeter(
		capsuleA_radius + capsuleB_radius,
		impl::ClosestPointLineLine(
			capsuleA_start, capsuleA_end, capsuleB_start, capsuleB_end, s, t, c1, c2
		)
	);
}

bool OverlapPolygonPolygon(
	const Transform& t1, const Polygon& A, const Transform& t2, const Polygon& B
) {
#ifdef PTGN_DEBUG
	game.stats.overlap_polygon_polygon++;
#endif
	PTGN_ASSERT(
		impl::IsConvexPolygon(A.vertices.data(), A.vertices.size()),
		"PolygonPolygon overlap check only works if both polygons are convex"
	);
	PTGN_ASSERT(
		impl::IsConvexPolygon(B.vertices.data(), B.vertices.size()),
		"PolygonPolygon overlap check only works if both polygons are convex"
	);
	return impl::PolygonsHaveOverlapAxis(t1, A, t2, B) &&
		   impl::PolygonsHaveOverlapAxis(t2, B, t1, A);
}

bool OverlapPolygonCapsule(
	const Transform& t1, const Polygon& A, const Transform& t2, const Capsule& B
) {
	auto capsule_radius{ B.GetRadius(t2) };

	if (capsule_radius <= 0.0f) {
		return false;
	}

	auto world_polygon{ A.GetWorldVertices(t1) };
	auto [capsule_start, capsule_end] = B.GetWorldVertices(t2);

	std::size_t vertex_count{ world_polygon.size() };

	for (std::size_t i{ 0 }; i < vertex_count; ++i) {
		if (OverlapLineCapsule(
				Transform{}, Line{ world_polygon[i], world_polygon[(i + 1) % vertex_count] },
				Transform{}, Capsule{ capsule_start, capsule_end, capsule_radius }
			)) {
			return true;
		}
	}
	return false;
}

} // namespace impl

bool Overlap(const Transform& t1, const Shape& shape1, const Transform& t2, const Shape& shape2) {
	return std::visit(
		[&](const auto& s1) -> bool {
			return std::visit(
				[&](const auto& s2) -> bool {
					using S1 = std::decay_t<decltype(s1)>;
					using S2 = std::decay_t<decltype(s2)>;
					PTGN_OVERLAP_SHAPE_PAIR_TABLE {
						PTGN_ERROR("Cannot find overlap function for the given shapes");
					}
				},
				shape2
			);
		},
		shape1
	);
}

bool Overlap(const Point& point, const Transform& t2, const Shape& shape2) {
	return Overlap(Transform{}, point, t2, shape2);
}

bool Overlap(const Transform& t1, const Shape& shape1, const Point& point) {
	return Overlap(point, t1, shape1);
}

} // namespace ptgn