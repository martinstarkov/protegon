
#include "math/geometry/triangle.h"

#include <array>

#include "ecs/components/transform.h"
#include "math/vector2.h"

namespace ptgn {

Triangle::Triangle(V2_float a, V2_float b, V2_float c) :
	a{ a }, b{ b }, c{ c } {}

Triangle::Triangle(const std::array<V2_float, 3>& vertices) :
	a{ vertices[0] }, b{ vertices[1] }, c{ vertices[2] } {}

std::array<V2_float, 3> Triangle::GetWorldVertices(const Transform& transform) const {
	auto local_vertices{ GetLocalVertices() };
	return transform.Apply(local_vertices);
}

std::array<V2_float, 3> Triangle::GetLocalVertices() const {
	return { a, b, c };
}

} // namespace ptgn