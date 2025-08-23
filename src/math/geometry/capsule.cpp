#include "math/geometry/capsule.h"

#include <array>

#include "components/draw.h"
#include "components/transform.h"
#include "core/entity.h"
#include "math/geometry.h"
#include "math/vector2.h"
#include "renderer/render_data.h"

namespace ptgn {

Capsule::Capsule(const V2_float& start, const V2_float& end, float radius) :
	start{ start }, end{ end }, radius{ radius } {}

void Capsule::Draw(impl::RenderData& ctx, const Entity& entity) {
	impl::DrawCapsule(ctx, entity);
}

std::array<V2_float, 2> Capsule::GetWorldVertices(const Transform& transform) const {
	auto local_vertices{ GetLocalVertices() };
	return ApplyTransform(local_vertices, transform);
}

std::array<V2_float, 2> Capsule::GetLocalVertices() const {
	return { start, end };
}

float Capsule::GetRadius() const {
	return radius;
}

float Capsule::GetRadius(const Transform& transform) const {
	return GetRadius() * transform.GetAverageScale();
}

} // namespace ptgn