#pragma once

#include <vector>

#include "protegon/polygon.h"
#include "protegon/vector2.h"

namespace ptgn::impl {

[[nodiscard]] float TriangulateArea(const V2_float* contour, std::size_t count);

// InsideTriangle decides if a point P is Inside of the triangle defined by A, B, C.
[[nodiscard]] bool TriangulateInsideTriangle(
	float Ax, float Ay, float Bx, float By, float Cx, float Cy, float Px, float Py
);

[[nodiscard]] bool TriangulateSnip(
	const V2_float* contour, int u, int v, int w, int n, const std::vector<int>& V
);

[[nodiscard]] std::vector<Triangle<float>> Triangulate(const V2_float* contour, std::size_t count);

} // namespace ptgn::impl