#include "math/geometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

#include "common/assert.h"
#include "math/tolerance.h"
#include "math/vector2.h"

namespace ptgn {

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

} // namespace impl

} // namespace ptgn