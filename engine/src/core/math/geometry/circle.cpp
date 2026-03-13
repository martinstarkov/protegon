#include "core/math/geometry/circle.h"

#include <array>
#include <cstdlib>

#include "core/assert.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"

namespace ptgn {

Circle::Circle(float circle_radius) : radius{ circle_radius } {}

V2_float Circle::GetCenter(Transform transform) const {
	return transform.GetPosition();
}

float Circle::GetRadius() const {
	return radius;
}

float Circle::GetRadius(Transform transform) const {
	auto radius{ GetRadius() };
	auto scale{ transform.GetAverageScale() };
	return radius * std::abs(scale);
}

std::array<V2_float, 4> Circle::GetWorldQuadVertices(Transform transform) const {
	auto local_vertices{ GetLocalQuadVertices() };
	return transform.Apply(local_vertices);
}

std::array<V2_float, 4> Circle::GetLocalQuadVertices() const {
	V2_float min{ -radius };
	V2_float max{ radius };
	PTGN_ASSERT(min != max, "Cannot get local vertices for a circle with size zero");
	return { min, V2_float{ max.x, min.y }, max, V2_float{ min.x, max.y } };
}

} // namespace ptgn