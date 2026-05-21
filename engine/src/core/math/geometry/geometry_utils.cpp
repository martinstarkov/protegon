#include "core/math/geometry/geometry_utils.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <set>
#include <span>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/math/angle.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/triangle.h"
#include "core/math/math_utils.h"
#include "core/math/tolerance.h"
#include "core/math/vector2.h"

namespace ptgn {

namespace impl {

std::vector<V2_float> GetArcVertices(
	V2_float center, float radius, Radians start_angle, Radians end_angle, bool clockwise
) {
	if (start_angle.value > end_angle.value) {
		end_angle += Radians{ kTwoPi };
	}

	auto arc_angle{ end_angle - start_angle };

	PTGN_ASSERT(arc_angle.value >= 0.0f);

	// Resolution indicates the number of vertices the arc is made up of. Each consecutive vertex,
	// alongside the center of the arc, makes up a triangle which is used to draw solid arcs.
	auto resolution{ std::max(360uz, static_cast<std::size_t>(30.0f * radius)) };

	PTGN_ASSERT(
		resolution > 1, "Arc must be made up of at least two vertices (forming one triangle with "
						"the arc center point)"
	);

	auto delta_angle{ arc_angle / static_cast<float>(resolution) };

	std::vector<V2_float> vertices(resolution);

	auto step{ clockwise ? -delta_angle : delta_angle };
	auto angle{ start_angle };

	for (auto& vertex : vertices) {
		vertex	= center + radius * V2_float{ angle.Cos(), angle.Sin() };
		angle  += step;
	}

	return vertices;
}

float TriangulateArea(std::span<const V2_float> vertices) {
	auto count{ vertices.size() };

	if (count < 3) {
		return 0.0f; // Not a polygon
	}

	float area{ 0.0f };

	for (auto i{ 0uz }; i < count; ++i) {
		auto current{ vertices[i] };
		auto next{ vertices[(i + 1) % count] };

		area += current.Cross(next);
	}

	return area * 0.5f;
}

bool TriangulateInsideTriangle(V2_float A, V2_float B, V2_float C, V2_float P) {
	return (C - B).Cross(P - B) >= 0.0f && (A - C).Cross(P - C) >= 0.0f &&
		   (B - A).Cross(P - A) >= 0.0f;
}

bool TriangulateSnip(
	const V2_float* contour, std::size_t u, std::size_t v, std::size_t w, std::size_t n,
	const std::vector<std::size_t>& V
) {
	PTGN_ASSERT(contour);

	auto A{ contour[V[u]] };
	auto B{ contour[V[v]] };
	auto C{ contour[V[w]] };

	auto AB{ B - A };
	auto AC{ C - A };

	if (float cross{ AB.Cross(AC) }; NearlyEqual(cross, 0.0f)) {
		return false;
	}

	for (auto i{ 0uz }; i < n; ++i) {
		if ((i == u) || (i == v) || (i == w)) {
			continue;
		}
		auto P{ contour[V[i]] };
		if (TriangulateInsideTriangle(A, B, C, P)) {
			return false;
		}
	}

	return true;
}

std::vector<Triangle> Triangulate(std::span<const V2_float> vertices) {
	// From: https://www.flipcode.com/archives/Efficient_Polygon_Triangulation.shtml

	std::vector<Triangle> result;

	auto n{ vertices.size() };

	if (n < 3) {
		return result;
	}

	std::vector<std::size_t> V(n);

	if (impl::TriangulateArea(vertices) > 0) {
		for (auto i{ 0uz }; i < n; ++i) {
			V[i] = i;
		}
	} else {
		for (auto i{ 0uz }; i < n; ++i) {
			V[i] = (n - 1) - i;
		}
	}

	std::size_t nv{ n };

	// Remove nv-2 Vertices, creating 1 triangle every time
	std::int64_t r_count{ 2 * static_cast<std::int64_t>(nv) }; // Error detection

	for ([[maybe_unused]] auto m{ 0uz }, v = nv - 1; nv > 2;) {
		// If we loop, it is probably a non-simple polygon
		if ((r_count--) < 0) {
			// Triangulate: ERROR - probable bad polygon
			return result;
		}

		// Three consecutive vertices in current polygon, <u,v,w>
		auto u{ v };
		if (nv <= u) {
			u = 0; // previous
		}
		v = u + 1;
		if (nv <= v) {
			v = 0; // new v
		}
		auto w{ v + 1 };
		if (nv <= w) {
			w = 0; // next
		}

		if (TriangulateSnip(vertices.data(), u, v, w, nv, V)) {
			// True names of the vertices
			auto a{ V[u] };
			auto b{ V[v] };
			auto c{ V[w] };

			result.emplace_back(vertices[a], vertices[b], vertices[c]);

			m++;

			// Remove v from remaining polygon
			for (auto i{ v + 1 }; i < nv; ++i) {
				auto s{ i - 1 };
				PTGN_ASSERT(s < V.size());
				PTGN_ASSERT(i < V.size());
				V[s] = V[i];
			}
			nv--;

			// Reset error detection counter
			r_count = 2 * static_cast<std::int64_t>(nv);
		}
	}

	return result;
}

Orientation GetOrientation(V2_float a, V2_float b, V2_float c) {
	auto det{ (b - a).Cross(c - a) };

	return static_cast<Orientation>(
		static_cast<int>(StrictlyLess(0.0f, det)) - static_cast<int>(StrictlyLess(det, 0.0f))
	);
}

bool VisibilityRayIntersects(
	V2_float origin, V2_float direction, const Line& segment, V2_float& out_point
) {
	auto ao{ origin - segment.GetStart() };
	auto ab{ segment.GetDirection() };
	auto det{ ab.Cross(direction) };

	if (NearlyEqual(det, 0.f)) {
		if (GetOrientation(segment.GetStart(), segment.GetEnd(), origin) !=
			Orientation::Collinear) {
			return false;
		}

		auto dist_a{ ao.Dot(direction) };
		auto dist_b{ (origin - segment.GetEnd()).Dot(direction) };

		if (dist_a > 0 && dist_b > 0) {
			return false;
		} else if ((dist_a > 0) != (dist_b > 0)) {
			out_point = origin;
		} else if (dist_a > dist_b) {		// at this point, both distances are negative
			out_point = segment.GetStart(); // hence the nearest point is A
		} else {
			out_point = segment.GetEnd();
		}

		return true;
	}

	if (auto u{ ao.Cross(direction) / det }; StrictlyLess(u, 0.0f) || StrictlyLess(1.0f, u)) {
		return false;
	}

	auto t = -(ab.Cross(ao)) / det;

	out_point = origin + t * direction;

	return NearlyEqual(t, 0.0f) || t > 0;
}

} // namespace impl

std::vector<V2_float> GetVisibilityPolygon(
	V2_float point, const std::vector<Line>& shadow_segments
) {
	using namespace ptgn::impl;

	// Compare 2 line segments based on their distance from given point.
	// Assumes: (1) The line segments are intersected by some ray from the origin.
	//          (2) The line segments do not intersect except at their endpoints.
	//          (3) No line segment is Collinear with the origin.
	// Check whether the line segment x is closer to the origin than the line segment y.
	// @param x Line segment: Left hand side of the comparison operator.
	// @param y Line segment: Right hand side of the comparison operator.
	// @return True if x < y (x is closer than y).
	//
	const auto cmp_dist = [origin = point](const Line& x, const Line& y) {
		auto [a, b] = x.GetLocalVertices();
		auto [c, d] = y.GetLocalVertices();

		PTGN_ASSERT(
			GetOrientation(origin, a, b) != Orientation::Collinear,
			"AB must not be Collinear with the origin."
		);
		PTGN_ASSERT(
			GetOrientation(origin, c, d) != Orientation::Collinear,
			"CD must not be Collinear with the origin."
		);

		// Sort the endpoints so that if there are common endpoints, it will be a and c.
		if (b == c || b == d) {
			std::swap(a, b);
		}
		if (a == d) {
			std::swap(c, d);
		}

		// Cases with common endpoints.
		if (a == c) {
			if (b == d || GetOrientation(origin, a, d) != GetOrientation(origin, a, b)) {
				return false;
			}
			return GetOrientation(a, b, d) != GetOrientation(a, b, origin);
		}

		// Cases without common endpoints.
		auto cda{ GetOrientation(c, d, a) };
		auto cdb{ GetOrientation(c, d, b) };

		if (cdb == Orientation::Collinear && cda == Orientation::Collinear) {
			return (origin - a).MagnitudeSquared() < (origin - c).MagnitudeSquared();
		} else if (cda == cdb || cda == Orientation::Collinear || cdb == Orientation::Collinear) {
			auto cdo = GetOrientation(c, d, origin);
			return cdo == cda || cdo == cdb;
		} else {
			auto abo = GetOrientation(a, b, origin);
			return abo != GetOrientation(a, b, c);
		}
	};

	std::set<Line, decltype(cmp_dist)> state{ cmp_dist };
	std::vector<VisibilityEvent> events;

	for (const auto& segment : shadow_segments) {
		// Sort line segment endpoints and add them as events.
		// Skip line segments Collinear with the point.
		if (auto pab{ GetOrientation(point, segment.GetStart(), segment.GetEnd()) };
			pab == Orientation::Collinear) {
			continue;
		} else if (pab == Orientation::RightTurn) {
			events.emplace_back(VisibilityEvent::Type::StartVertex, segment);
			events.emplace_back(
				VisibilityEvent::Type::EndVertex, Line{ segment.GetEnd(), segment.GetStart() }
			);
		} else {
			events.emplace_back(
				VisibilityEvent::Type::StartVertex, Line{ segment.GetEnd(), segment.GetStart() }
			);
			events.emplace_back(VisibilityEvent::Type::EndVertex, segment);
		}

		// Initialize state by adding line segments that are intersected
		// by vertical ray from the point.
		auto [a, b] = segment.GetLocalVertices();

		if (a.x > b.x) {
			std::swap(a, b);
		}

		if (GetOrientation(a, b, point) == Orientation::RightTurn &&
			(NearlyEqual(b.x, point.x) || (a.x < point.x && point.x < b.x))) {
			state.insert(segment);
		}
	}

	// compare angles clockwise starting at the positive y axis
	const auto angle_comparer = [point](V2_float a, V2_float b) {
		auto is_a_left{ StrictlyLess(a.x, point.x) };
		auto is_b_left{ StrictlyLess(b.x, point.x) };

		if (is_a_left != is_b_left) {
			return is_b_left;
		}

		if (NearlyEqual(a.x, point.x) && NearlyEqual(b.x, point.x)) {
			if (!StrictlyLess(a.y, point.y) || !StrictlyLess(b.y, point.y)) {
				return StrictlyLess(b.y, a.y);
			}
			return StrictlyLess(a.y, b.y);
		}

		auto oa{ a - point };
		auto ob{ b - point };
		auto det{ oa.Cross(ob) };

		if (NearlyEqual(det, 0.f)) {
			return oa.MagnitudeSquared() < ob.MagnitudeSquared();
		}

		return det < 0;
	};

	// Sort events by angle.
	std::sort(events.begin(), events.end(), [&angle_comparer](const auto& a, const auto& b) {
		// If the points are equal, sort end vertices first.
		if (a.segment.GetStart() == b.segment.GetStart()) {
			return a.type == VisibilityEvent::Type::EndVertex &&
				   b.type == VisibilityEvent::Type::StartVertex;
		}
		return angle_comparer(a.segment.GetStart(), b.segment.GetStart());
	});

	// Find the visibility polygon.
	std::vector<V2_float> vertices;

	for (const auto& event : events) {
		if (event.type == VisibilityEvent::Type::EndVertex) {
			state.erase(event.segment);
		}

		if (state.empty()) {
			vertices.emplace_back(event.segment.GetStart());
		} else if (cmp_dist(event.segment, *state.begin())) {
			// Nearest line segment has changed.
			// Compute the intersection point with this segment.
			V2_float intersection;
			Line nearest_segment{ *state.begin() };
			[[maybe_unused]] auto intersects{ VisibilityRayIntersects(
				point, event.segment.GetStart() - point, nearest_segment, intersection
			) };

			// TODO: Readd this assert once the resolution change no longer crashes the algorithm.
			// PTGN_ASSERT(intersects, "Ray intersects line segment L if L is in the state");

			if (event.type == VisibilityEvent::Type::StartVertex) {
				vertices.emplace_back(intersection);
				vertices.emplace_back(event.segment.GetStart());
			} else {
				vertices.emplace_back(event.segment.GetStart());
				vertices.emplace_back(intersection);
			}
		}

		if (event.type == VisibilityEvent::Type::StartVertex) {
			state.insert(event.segment);
		}
	}

	auto top{ vertices.begin() };

	// Remove collinear points.
	for (auto it{ vertices.begin() }; it != vertices.end(); ++it) {
		auto prev{ top == vertices.begin() ? vertices.end() - 1 : top - 1 };
		auto next{ it + 1 == vertices.end() ? vertices.begin() : it + 1 };

		if (GetOrientation(*prev, *it, *next) != Orientation::Collinear) {
			*top++ = *it;
		}
	}
	vertices.erase(top, vertices.end());
	return vertices;
}

std::vector<Triangle> GetVisibilityTriangles(
	V2_float origin, const std::vector<Line>& shadow_segments
) {
	auto polygon{ GetVisibilityPolygon(origin, shadow_segments) };

	auto count{ polygon.size() };

	// We need at least 3 points to form a triangle.
	if (count < 3) {
		return {};
	}

	std::vector<Triangle> triangles;
	triangles.reserve(count);

	for (auto i{ 0uz }; i < count; ++i) {
		V2_float a{ polygon[i] };
		V2_float b{ polygon[(i + 1) % count] };

		triangles.emplace_back(origin, a, b);
	}

	return triangles;
}

std::vector<Line> PointsToLines(const std::vector<V2_float>& points, bool connect_last_to_first) {
	std::size_t count{ points.size() };

	if (count < 2) {
		return {};
	}

	std::size_t end{ connect_last_to_first ? count : count - 1 };

	std::vector<Line> lines;
	lines.reserve(end);

	for (auto i{ 0uz }; i < end; ++i) {
		// Wraps around if connect_last_to_first is true.
		lines.emplace_back(points[i], points[(i + 1) % count]);
	}
	return lines;
}

namespace impl {

bool IsInside(V2_float p, const Line& edge) {
	V2_float edge_vec{ edge.GetDirection() };
	V2_float point_vec{ p - edge.GetStart() };

	// Cross product >= 0 means p is to the left or on the edge line.
	return edge_vec.Cross(point_vec) >= 0;
}

std::optional<V2_float> ComputeIntersection(V2_float a, V2_float b, V2_float c, V2_float d) {
	V2_float ab{ b - a };
	V2_float cd{ d - c };

	float denominator{ ab.Cross(cd) };

	if (std::abs(denominator) < kEpsilon<float>) {
		return std::nullopt; // Lines are parallel.
	}

	float t{ (c - a).Cross(cd) / denominator };

	if (t < 0.0f || t > 1.0f) {
		return std::nullopt; // Intersection not within segment AB.
	}

	return a + ab * t;
}

} // namespace impl

std::vector<V2_float> ClipPolygons(
	const std::vector<V2_float>& subject_polygon, const std::vector<V2_float>& clip_polygon
) {
	std::vector<V2_float> output_list{ subject_polygon };

	auto count{ clip_polygon.size() };

	for (auto i{ 0uz }; i < count; ++i) {
		V2_float clip_start = clip_polygon[i];
		V2_float clip_end	= clip_polygon[(i + 1) % count];

		Line clip_edge{ clip_start, clip_end };

		std::vector<V2_float> input_list{ output_list };

		output_list.clear();

		if (input_list.empty()) {
			break;
		}

		V2_float s{ input_list.back() };

		for (V2_float e : input_list) {
			bool e_inside{ impl::IsInside(e, clip_edge) };
			bool s_inside{ impl::IsInside(s, clip_edge) };

			if (e_inside) {
				if (!s_inside) {
					if (auto intersection{ impl::ComputeIntersection(
							s, e, clip_edge.GetStart(), clip_edge.GetEnd()
						) }) {
						output_list.push_back(*intersection);
					}
				}
				output_list.push_back(e);
			} else if (s_inside) {
				if (auto intersection{ impl::ComputeIntersection(
						s, e, clip_edge.GetStart(), clip_edge.GetEnd()
					) }) {
					output_list.push_back(*intersection);
				}
			}

			s = e;
		}
	}

	return output_list;
}

namespace impl {

bool WithinPerimeter(float radius, float dist2, bool include_edge) {
	float radius2 = radius * radius;
	if (dist2 < radius2) {
		return true;
	}
	return include_edge && NearlyEqual(dist2, radius2);
}

float ClosestPointLineLine(
	V2_float lineA_start, V2_float lineA_end, V2_float lineB_start, V2_float lineB_end, float& s,
	float& t, V2_float& c1, V2_float& c2
) {
	V2_float d1{ lineA_end - lineA_start }; // Direction vector of segment S1
	V2_float d2{ lineB_end - lineB_start }; // Direction vector of segment S2
	V2_float r{ lineA_start - lineB_start };
	float a = d1.Dot(d1);					// Squared length of segment S1, always nonnegative
	float e = d2.Dot(d2);					// Squared length of segment S2, always nonnegative
	float f = d2.Dot(r);
	// Checke if one or both segments degenerate into points.
	if (a <= kEpsilon<float> && e <= kEpsilon<float>) {
		// Both segments degenerate into points
		s = t = 0.0f;
		c1	  = lineA_start;
		c2	  = lineB_start;
		return (c1 - c2).Dot(c1 - c2);
	}
	if (a <= kEpsilon<float>) {
		// First segment degenerates into a point
		s = 0.0f;
		t = f / e; // s = 0 => t = (b*s + f) / e = f / e
		t = Clamp01(t);
	} else {
		float c = d1.Dot(r);
		if (e <= kEpsilon<float>) {
			// Second segment degenerates into a point
			t = 0.0f;
			s = Clamp01(-c / a); // t = 0 => s = (b*t - c) / a = -c / a
		} else {
			// The general nondegenerate case starts here
			float b		= d1.Dot(d2);
			float denom = a * e - b * b; // Always nonnegative
			// If segments not parallel, compute closest point on L1 to L2 and
			// clamp to segment S1. Else pick arbitrary s (here 0)
			if (denom != 0.0f) {
				s = Clamp01((b * f - c * e) / denom);
			} else {
				s = 0.0f;
			}

			// Compute point on L2 closest to S1(s) using
			// t = Dot((P1 + D1*s) - P2,D2) / Dot(D2,D2) = (b*s + f) / e
			float tnom = b * s + f;

			if (tnom < 0.0f) {
				t = 0.0f;
				s = Clamp01(-c / a);
			} else if (tnom > e) {
				t = 1.0f;
				s = Clamp01((b - c) / a);
			} else {
				t = tnom / e;
			}
		}
	}
	c1 = lineA_start + d1 * s;
	c2 = lineB_start + d2 * t;
	return (c1 - c2).Dot(c1 - c2);
}

float SquareDistancePointLine(V2_float point, V2_float start, V2_float end) {
	// Source:
	// https://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 130.
	V2_float ab{ end - start };
	V2_float ac{ point - start };
	V2_float bc{ point - end };
	float e = ac.Dot(ab);
	// Handle cases where c projects outside ab
	if (e <= 0.0f) {
		return ac.Dot(ac);
	}
	float f = ab.Dot(ab);
	if (e >= f) {
		return bc.Dot(bc);
	}
	// Handle cases where c projects onto ab
	return ac.Dot(ac) - e * e / f;
}

float SquareDistancePointRect(V2_float point, V2_float rect_min, V2_float rect_max) {
	float dist2{ 0.0f };
	for (auto i{ 0uz }; i < 2; ++i) {
		const float v{ point[i] };
		if (v < rect_min[i]) {
			dist2 += (rect_min[i] - v) * (rect_min[i] - v);
		}
		if (v > rect_max[i]) {
			dist2 += (v - rect_max[i]) * (v - rect_max[i]);
		}
	}
	return dist2;
}

float ParallelogramArea(V2_float a, V2_float b, V2_float c) {
	return (a - c).Cross(b - c);
}

bool IntervalsOverlap(float min1, float max1, float min2, float max2) {
	return !(min1 > max2 || min2 > max1);
}

float GetIntervalOverlap(
	float min1, float max1, float min2, float max2, bool contained_polygon,
	V2_float& out_axis_direction
) {
	// TODO: Combine the contained_polygon case into the regular overlap logic if possible.

	if (!IntervalsOverlap(min1, max1, min2, max2)) {
		return 0.0f;
	}

	float min_dist{ min1 - min2 };
	float max_dist{ max1 - max2 };

	if (contained_polygon) {
		float internal_dist{ std::min(max1, max2) - std::max(min1, min2) };

		// Get the overlap plus the distance from the minimum end points.
		float min_endpoint{ std::abs(min_dist) };
		float max_endpoint{ std::abs(max_dist) };

		if (max_endpoint > min_endpoint) {
			// Flip projection normal direction.
			out_axis_direction *= -1.0f;
			return internal_dist + min_endpoint;
		}
		return internal_dist + max_endpoint;
	}

	float right_dist{ std::abs(min1 - max2) };

	if (max_dist > 0.0f) { // Overlapping the interval from the right.
		return right_dist;
	}

	float left_dist{ std::abs(max1 - min2) };

	if (min_dist < 0.0f) { // Overlapping the interval from the left.
		return left_dist;
	}

	// Entirely within the interval.
	return std::min(right_dist, left_dist);
}

bool IsConvexPolygon(std::span<const V2_float> vertices) {
	auto count{ vertices.size() };

	PTGN_ASSERT(count >= 3, "Line or point convexity check is redundant");

	const auto get_cross = [](V2_float a, V2_float b, V2_float c) {
		return (b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x);
	};

	int sign{ static_cast<int>(Sign(get_cross(vertices[0], vertices[1], vertices[2]))) };

	// For convex polygons, all sequential point triplet cross products must have the same sign
	// (+ or -). For convex polygon every triplet makes turn in the same side (or CW, or CCW
	// depending on walk direction). For concave one some signs will differ (where inner angle
	// exceeds 180 degrees). Note that you don't need to calculate angle values. Source:
	// https://stackoverflow.com/a/40739079

	// Skip first point since that is the established reference.
	for (auto i{ 1uz }; i < count; ++i) {
		auto a{ vertices[i + 0] };
		auto b{ vertices[(i + 1) % count] };
		auto c{ vertices[(i + 2) % count] };

		auto new_sign{ static_cast<int>(Sign(get_cross(a, b, c))) };

		if (new_sign != sign) {
			// Polygon is concave.
			return false;
		}
	}

	// Convex.
	return true;
}

bool IsConcavePolygon(std::span<const V2_float> vertices) {
	return !IsConvexPolygon(vertices);
}

} // namespace impl

} // namespace ptgn