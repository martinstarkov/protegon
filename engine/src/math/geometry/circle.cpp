#include "math/geometry/circle.h"

#include <array>

#include "ecs/components/draw.h"
#include "ecs/components/transform.h"
#include "ecs/entity.h"
#include "math/math_utils.h"
#include "math/vector2.h"

namespace ptgn {

Circle::Circle(float circle_radius) : radius{ circle_radius } {}

void Circle::Draw(const Entity& entity) {
	// TODO: Fix.
	// impl::DrawCircle(entity);
}

V2_float Circle::GetCenter(const Transform& transform) const {
	return transform.GetPosition();
}

float Circle::GetRadius() const {
	return radius;
}

float Circle::GetRadius(const Transform& transform) const {
	return GetRadius() * Abs(transform.GetAverageScale());
}

std::array<V2_float, 4> Circle::GetWorldQuadVertices(const Transform& transform) const {
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