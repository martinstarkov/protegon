#include "math/geometry/circle.h"

#include <array>

#include "components/draw.h"
#include "components/transform.h"
#include "core/entity.h"
#include "math/vector2.h"

namespace ptgn {

Circle::Circle(float circle_radius) : radius{ circle_radius } {}

void Circle::Draw(const Entity& entity) {
	impl::DrawCircle(entity);
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

std::array<V2_float, 2> Circle::GetExtents(const Transform& transform) const {
	V2_float c{ GetCenter(transform) };
	V2_float r{ GetRadius(transform) };
	return { c - r, c + r };
}

} // namespace ptgn