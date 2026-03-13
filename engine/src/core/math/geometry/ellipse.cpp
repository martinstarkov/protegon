#include "core/math/geometry/ellipse.h"

#include <array>

#include "core/assert.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"

namespace ptgn {

V2_float Ellipse::GetCenter(Transform transform) const {
	return transform.GetPosition();
}

V2_float Ellipse::GetRadius() const {
	return radius;
}

V2_float Ellipse::GetRadius(Transform transform) const {
	auto radius{ GetRadius() };
	auto scale{ transform.GetScale() };
	return radius * Abs(scale);
}

std::array<V2_float, 4> Ellipse::GetWorldQuadVertices(Transform transform) const {
	auto local_vertices{ GetLocalQuadVertices() };
	return transform.Apply(local_vertices);
}

std::array<V2_float, 4> Ellipse::GetLocalQuadVertices() const {
	auto min{ -radius };
	auto max{ radius };
	PTGN_ASSERT(min != max, "Cannot get local vertices for a ellipse with size zero");
	return { min, V2_float{ max.x, min.y }, max, V2_float{ min.x, max.y } };
}

} // namespace ptgn