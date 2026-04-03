
#include "core/math/geometry/triangle.h"

#include <array>

#include "core/math/transform.h"
#include "core/math/vector2.h"

namespace ptgn {

Triangle::Triangle(V2_float a, V2_float b, V2_float c) : vertices_{ a, b, c } {}

Triangle::Triangle(const std::array<V2_float, 3>& vertices) : vertices_{ vertices } {}

void Triangle::SetVertices(V2_float a, V2_float b, V2_float c) {
	vertices_ = { a, b, c };
}

std::array<V2_float, 3> Triangle::GetLocalVertices() const {
	return vertices_;
}

std::array<V2_float, 3> Triangle::GetWorldVertices(Transform transform) const {
	auto local_vertices{ GetLocalVertices() };
	return transform.Apply(local_vertices);
}

std::array<V2_float, 4> Triangle::GetWorldQuadVertices(Transform transform) const {
	auto vertices{ GetWorldVertices(transform) };
	std::array<V2_float, 4> quad_vertices{ vertices[0], vertices[1], vertices[2], vertices[0] };
	return quad_vertices;
}

} // namespace ptgn