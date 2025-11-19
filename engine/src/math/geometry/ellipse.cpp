#include "math/geometry/ellipse.h"

#include <array>

#include "core/assert.h"
#include "ecs/components/draw.h"
#include "ecs/components/transform.h"
#include "ecs/entity.h"
#include "math/vector2.h"

namespace ptgn {

void Ellipse::Draw(const Entity& entity) {
	// TODO: Fix.
	// impl::DrawEllipse(entity);
}

V2_float Ellipse::GetCenter(const Transform& transform) const {
	return transform.GetPosition();
}

V2_float Ellipse::GetRadius() const {
	return radius;
}

V2_float Ellipse::GetRadius(const Transform& transform) const {
	return GetRadius() * Abs(transform.GetScale());
}

std::array<V2_float, 4> Ellipse::GetWorldQuadVertices(const Transform& transform) const {
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