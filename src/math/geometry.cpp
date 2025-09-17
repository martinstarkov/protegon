#include "math/geometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <set>
#include <span>
#include <utility>
#include <variant>
#include <vector>

#include "common/assert.h"
#include "math/geometry/line.h"
#include "math/geometry/triangle.h"
#include "math/tolerance.h"
#include "math/vector2.h"

namespace ptgn {

bool StrictlyLess(float a, float b, float epsilon) {
	return (b - a) > std::max(std::abs(a), std::abs(b)) * epsilon;
}

bool StrictlyLess(const V2_float& a, const V2_float& b, float epsilon) {
	return StrictlyLess(a.x, b.x, epsilon) && StrictlyLess(a.y, b.y, epsilon);
}

namespace impl {

std::vector<V2_float> GetArcVertices(
	const V2_float& center, float radius, float start_angle, float end_angle, bool clockwise
) {
	if (start_angle > end_angle) {
		end_angle += two_pi<float>;
	}

	float arc_angle{ end_angle - start_angle };

	PTGN_ASSERT(arc_angle >= 0.0f);

	// Resolution indicates the number of vertices the arc is made up of. Each consecutive vertex,
	// alongside the center of the arc, makes up a triangle which is used to draw solid arcs.
	std::size_t resolution{
		std::max(static_cast<std::size_t>(360), static_cast<std::size_t>(30.0f * radius))
	};

	PTGN_ASSERT(
		resolution > 1, "Arc must be made up of at least two vertices (forming one triangle with "
						"the arc center point)"
	);

	float delta_angle{ arc_angle / static_cast<float>(resolution) };

	std::vector<V2_float> vertices(resolution);

	for (std::size_t i{ 0 }; i < vertices.size(); i++) {
		float angle{ start_angle };
		float delta{ static_cast<float>(i) * delta_angle };
		if (clockwise) {
			angle -= delta;
		} else {
			angle += delta;
		}

		vertices[i] = center + radius * V2_float{ std::cos(angle), std::sin(angle) };
	}

	return vertices;
}

float TriangulateArea(std::span<const V2_float> vertices) {
	auto count{ vertices.size() };

	if (count < 3) {
		return 0.0f; // Not a polygon
	}

	float area{ 0.0f };

	for (std::size_t i{ 0 }; i < count; ++i) {
		V2_float current{ vertices[i] };
		V2_float next{ vertices[(i + 1) % count] };

		area += current.Cross(next);
	}

	return area * 0.5f;
}

bool TriangulateInsideTriangle(
	const V2_float& A, const V2_float& B, const V2_float& C, const V2_float& P
) {
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

	for (std::size_t p{ 0 }; p < n; p++) {
		if ((p == u) || (p == v) || (p == w)) {
			continue;
		}
		auto P{ contour[V[p]] };
		if (TriangulateInsideTriangle(A, B, C, P)) {
			return false;
		}
	}

	return true;
}

std::vector<std::array<V2_float, 3>> Triangulate(std::span<const V2_float> vertices) {
	// From: https://www.flipcode.com/archives/Efficient_Polygon_Triangulation.shtml

	std::vector<std::array<V2_float, 3>> result;

	auto n{ vertices.size() };

	if (n < 3) {
		return result;
	}

	std::vector<std::size_t> V(n);

	if (impl::TriangulateArea(vertices) > 0) {
		for (std::size_t v{ 0 }; v < n; v++) {
			V[v] = v;
		}
	} else {
		for (std::size_t v{ 0 }; v < n; v++) {
			V[v] = (n - 1) - v;
		}
	}

	auto nv{ n };

	/*  remove nv-2 Vertices, creating 1 triangle every time */
	std::int64_t r_count{ 2 * static_cast<std::int64_t>(nv) }; /* error detection */

	for ([[maybe_unused]] std::size_t m{ 0 }, v = nv - 1; nv > 2;) {
		/* if we loop, it is probably a non-simple polygon */
		if (0 > (r_count--)) {
			//** Triangulate: ERROR - probable bad polygon!
			return result;
		}

		/* three consecutive vertices in current polygon, <u,v,w> */
		auto u{ v };
		if (nv <= u) {
			u = 0; /* previous */
		}
		v = u + 1;
		if (nv <= v) {
			v = 0; /* new v    */
		}
		auto w{ v + 1 };
		if (nv <= w) {
			w = 0; /* next     */
		}

		if (TriangulateSnip(vertices.data(), u, v, w, nv, V)) {
			/* true names of the vertices */
			auto a{ V[u] };
			auto b{ V[v] };
			auto c{ V[w] };

			result.emplace_back(std::array<V2_float, 3>{ vertices[a], vertices[b], vertices[c] });

			m++;

			/* remove v from remaining polygon */
			for (auto t{ v + 1 }; t < nv; t++) {
				auto s{ t - 1 };
				PTGN_ASSERT(s < V.size());
				PTGN_ASSERT(t < V.size());
				V[s] = V[t];
			}
			nv--;

			/* resest error detection counter */
			r_count = 2 * nv;
		}
	}

	return result;
}

Orientation GetOrientation(const V2_float& a, const V2_float& b, const V2_float& c) {
	auto det{ (b - a).Cross(c - a) };

	return static_cast<Orientation>(
		static_cast<int>(StrictlyLess(0.0f, det)) - static_cast<int>(StrictlyLess(det, 0.0f))
	);
}

bool VisibilityRayIntersects(
	const V2_float& origin, const V2_float& direction, const Line& segment, V2_float& out_point
) {
	auto ao{ origin - segment.start };
	auto ab{ segment.end - segment.start };
	auto det{ ab.Cross(direction) };

	if (NearlyEqual(det, 0.f)) {
		if (GetOrientation(segment.start, segment.end, origin) != Orientation::Collinear) {
			return false;
		}

		auto dist_a{ ao.Dot(direction) };
		auto dist_b{ (origin - segment.end).Dot(direction) };

		if (dist_a > 0 && dist_b > 0) {
			return false;
		} else if ((dist_a > 0) != (dist_b > 0)) {
			out_point = origin;
		} else if (dist_a > dist_b) {  // at this point, both distances are negative
			out_point = segment.start; // hence the nearest point is A
		} else {
			out_point = segment.end;
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
	const V2_float& point, const std::vector<Line>& shadow_segments
) {
	using namespace ptgn::impl;

	/* Compare 2 line segments based on their distance from given point.
	 * Assumes: (1) The line segments are intersected by some ray from the origin.
	 *          (2) The line segments do not intersect except at their endpoints.
	 *          (3) No line segment is Collinear with the origin.
	 * Check whether the line segment x is closer to the origin than the line segment y.
	 * @param x Line segment: Left hand side of the comparison operator.
	 * @param y Line segment: Right hand side of the comparison operator.
	 * @return True if x < y (x is closer than y).
	 */
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
		if (auto pab{ GetOrientation(point, segment.start, segment.end) };
			pab == Orientation::Collinear) {
			continue;
		} else if (pab == Orientation::RightTurn) {
			events.emplace_back(VisibilityEvent::StartVertex, segment);
			events.emplace_back(VisibilityEvent::EndVertex, Line{ segment.end, segment.start });
		} else {
			events.emplace_back(VisibilityEvent::StartVertex, Line{ segment.end, segment.start });
			events.emplace_back(VisibilityEvent::EndVertex, segment);
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
	const auto angle_comparer = [point](const V2_float& a, const V2_float& b) {
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
		if (a.segment.start == b.segment.start) {
			return a.type == VisibilityEvent::EndVertex && b.type == VisibilityEvent::StartVertex;
		}
		return angle_comparer(a.segment.start, b.segment.start);
	});

	// Find the visibility polygon.
	std::vector<V2_float> vertices;

	for (const auto& event : events) {
		if (event.type == VisibilityEvent::EndVertex) {
			state.erase(event.segment);
		}

		if (state.empty()) {
			vertices.emplace_back(event.segment.start);
		} else if (cmp_dist(event.segment, *state.begin())) {
			// Nearest line segment has changed.
			// Compute the intersection point with this segment.
			V2_float intersection;
			Line nearest_segment{ *state.begin() };
			[[maybe_unused]] auto intersects{ VisibilityRayIntersects(
				point, event.segment.start - point, nearest_segment, intersection
			) };

			// TODO: Readd this assert once the resolution change no longer crashes the algorithm.
			// PTGN_ASSERT(intersects, "Ray intersects line segment L if L is in the state");

			if (event.type == VisibilityEvent::StartVertex) {
				vertices.emplace_back(intersection);
				vertices.emplace_back(event.segment.start);
			} else {
				vertices.emplace_back(event.segment.start);
				vertices.emplace_back(intersection);
			}
		}

		if (event.type == VisibilityEvent::StartVertex) {
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
	const V2_float& origin, const std::vector<Line>& shadow_segments
) {
	auto polygon{ GetVisibilityPolygon(origin, shadow_segments) };

	// We need at least 3 points to form a triangle.
	if (polygon.size() < 3) {
		return {};
	}

	std::vector<Triangle> triangles;
	triangles.reserve(polygon.size());

	for (std::size_t i = 0; i < polygon.size(); ++i) {
		V2_float a{ polygon[i] };
		V2_float b{ polygon[(i + 1) % polygon.size()] };

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

	for (std::size_t i{ 0 }; i < end; ++i) {
		// Wraps around if connect_last_to_first is true.
		lines.emplace_back(points[i], points[(i + 1) % count]);
	}
	return lines;
}

namespace impl {

bool IsInside(const V2_float& p, const Line& edge) {
	V2_float edge_vec{ edge.end - edge.start };
	V2_float point_vec{ p - edge.start };

	// Cross product >= 0 means p is to the left or on the edge line.
	return edge_vec.Cross(point_vec) >= 0;
}

std::optional<V2_float> ComputeIntersection(
	const V2_float& a, const V2_float& b, const V2_float& c, const V2_float& d
) {
	V2_float ab{ b - a };
	V2_float cd{ d - c };

	float denominator{ ab.Cross(cd) };

	if (std::abs(denominator) < epsilon<float>) {
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

	for (std::size_t i{ 0 }; i < clip_polygon.size(); ++i) {
		V2_float clip_start = clip_polygon[i];
		V2_float clip_end	= clip_polygon[(i + 1) % clip_polygon.size()];

		Line clip_edge{ clip_start, clip_end };

		std::vector<V2_float> input_list{ output_list };

		output_list.clear();

		if (input_list.empty()) {
			break;
		}

		V2_float s{ input_list.back() };

		for (const V2_float& e : input_list) {
			bool e_inside{ impl::IsInside(e, clip_edge) };
			bool s_inside{ impl::IsInside(s, clip_edge) };

			if (e_inside) {
				if (!s_inside) {
					if (auto intersection{
							impl::ComputeIntersection(s, e, clip_edge.start, clip_edge.end) }) {
						output_list.push_back(*intersection);
					}
				}
				output_list.push_back(e);
			} else if (s_inside) {
				if (auto intersection{
						impl::ComputeIntersection(s, e, clip_edge.start, clip_edge.end) }) {
					output_list.push_back(*intersection);
				}
			}

			s = e;
		}
	}

	return output_list;
}

} // namespace ptgn