#pragma once

#include <array>
#include <variant>
#include <vector>

#include "components/transform.h"
#include "math/geometry/capsule.h"
#include "math/geometry/circle.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/geometry/rect.h"
#include "math/geometry/triangle.h"
#include "math/vector2.h"

namespace ptgn {

class Entity;

using Point = V2_float;

using Shape = std::variant<Point, Rect, Circle, Polygon, Line, Triangle, Capsule>;

[[nodiscard]] Transform ApplyOffset(
	const Shape& shape, const Transform& transform, const Entity& entity
);

[[nodiscard]] V2_float ToWorldPoint(
	const V2_float& local_point, const V2_float& position, const V2_float& scale, float cos_angle,
	float sin_angle
);

[[nodiscard]] V2_float ToWorldPoint(
	const V2_float& local_point, const V2_float& position, const V2_float& scale
);

[[nodiscard]] V2_float ToWorldPoint(const V2_float& local_point, const Transform& transform);

void ToWorldPoint(
	const V2_float* local_points, std::size_t count, V2_float* out_world_points,
	const Transform& transform
);

std::vector<V2_float> ToWorldPoint(
	const std::vector<V2_float>& local_points, const Transform& transform
);

template <std::size_t N>
std::array<V2_float, N> ToWorldPoint(
	const std::array<V2_float, N>& local_points, const Transform& transform
) {
	std::array<V2_float, N> world_points;
	ToWorldPoint(local_points.data(), N, world_points.data(), transform);
	return world_points;
}

namespace impl {

// TODO: Move into Arc class.
// @param clockwise Whether the vertices are in clockwise direction (true), or counter-clockwise
// (false).
// @param start_angle Must be in range: [0, 2pi).
// @param end_angle Must be in range: [0, 2pi).
// @return The vertices which make up the arc.
[[nodiscard]] std::vector<V2_float> GetVertices(
	const V2_float& center, float radius, float start_angle, float end_angle, bool clockwise
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

} // namespace impl

} // namespace ptgn