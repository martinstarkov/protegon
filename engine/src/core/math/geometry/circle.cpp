#include "core/math/geometry/circle.h"

#include <array>
#include <cstdlib>

#include "core/assert.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"

namespace ptgn {

Circle::Circle(float radius) : radius_{ radius } {}

void Circle::SetRadius(float radius) {
	radius_ = radius;
}

V2_float Circle::GetCenter(Transform transform) const {
	return transform.GetPosition();
}

float Circle::GetRadius() const {
	return radius_;
}

V2_float Circle::GetSize() const {
	return V2_float{ radius_ } * 2.0f;
}

V2_float Circle::GetSize(Transform transform) const {
	auto circle_size{ GetSize() };
	auto scale{ transform.GetScale() };
	return circle_size * Abs(scale);
}

float Circle::GetRadius(Transform transform) const {
	auto circle_radius{ GetRadius() };
	auto scale{ transform.GetAverageScale() };
	return circle_radius * std::abs(scale);
}

std::array<V2_float, 4> Circle::GetWorldQuadVertices(Transform transform) const {
	auto local_vertices{ GetLocalQuadVertices() };
	return transform.Apply(local_vertices);
}

std::array<V2_float, 4> Circle::GetLocalQuadVertices() const {
	V2_float min{ -radius_ };
	V2_float max{ radius_ };
	PTGN_ASSERT(min != max, "Cannot get local vertices for a circle with size zero");
	return { min, V2_float{ max.x, min.y }, max, V2_float{ min.x, max.y } };
}

} // namespace ptgn