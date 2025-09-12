#pragma once

#include <array>
#include <span>
#include <vector>

#include "math/vector2.h"

namespace ptgn {

namespace impl {

// @param clockwise Whether the vertices are in clockwise direction (true), or counter-clockwise
// (false).
// @param start_angle Must be in range: [0, 2pi).
// @param end_angle Must be in range: [0, 2pi).
// @return The vertices which make up the arc.
[[nodiscard]] std::vector<V2_float> GetArcVertices(
	const V2_float& center, float radius, float start_angle, float end_angle, bool clockwise
);

[[nodiscard]] float TriangulateArea(std::span<const V2_float> vertices);

// InsideTriangle decides if a point P is Inside of the triangle defined by A, B, C.
[[nodiscard]] bool TriangulateInsideTriangle(
	const V2_float& A, const V2_float& B, const V2_float& C, const V2_float& P
);

[[nodiscard]] bool TriangulateSnip(
	const V2_float* contour, std::size_t u, std::size_t v, std::size_t w, std::size_t n,
	const std::vector<std::size_t>& V
);

// @return A vector of triangles which make up the polygon contour.
[[nodiscard]] std::vector<std::array<V2_float, 3>> Triangulate(std::span<const V2_float> vertices);

} // namespace impl

} // namespace ptgn