#pragma once

#include <array>
#include <vector>

#include "components/transform.h"
#include "math/vector2.h"
#include "rendering/api/origin.h"

namespace ptgn::impl {

[[nodiscard]] V2_float GetCenter(const V2_float* vertices, std::size_t vertex_count);
[[nodiscard]] V2_float GetCenter(const Transform& transform, V2_float size, Origin origin);

// @param clockwise Whether the vertices are in clockwise direction (true), or counter-clockwise
// (false).
// @param start_angle Must be in range: [0, 2pi).
// @param end_angle Must be in range: [0, 2pi).
// @return The vertices which make up the arc.
[[nodiscard]] std::vector<V2_float> GetVertices(
	const V2_float& center, float radius, float start_angle, float end_angle, bool clockwise
);

[[nodiscard]] std::array<V2_float, 4> GetQuadVertices(
	const V2_float& start, const V2_float& end, float line_width
);

[[nodiscard]] std::array<V2_float, 4> GetVertices(
	const Transform& transform, V2_float size, Origin origin
);

[[nodiscard]] float TriangulateArea(const V2_float* contour, std::size_t count);

// InsideTriangle decides if a point P is Inside of the triangle defined by A, B, C.
[[nodiscard]] bool TriangulateInsideTriangle(
	float Ax, float Ay, float Bx, float By, float Cx, float Cy, float Px, float Py
);

[[nodiscard]] bool TriangulateSnip(
	const V2_float* contour, int u, int v, int w, int n, const std::vector<int>& V
);

// @return A vector of triangles which make up the polygon contour.
[[nodiscard]] std::vector<std::array<V2_float, 3>> Triangulate(
	const V2_float* vertices, std::size_t vertex_count
);

} // namespace ptgn::impl