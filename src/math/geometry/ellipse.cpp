#include "math/geometry/ellipse.h"

#include <array>

#include "common/assert.h"
#include "components/draw.h"
#include "components/transform.h"
#include "core/entity.h"
#include "math/vector2.h"
#include "renderer/render_data.h"

namespace ptgn {

Ellipse::Ellipse(const V2_float& ellipse_radius) : radius{ ellipse_radius } {}

void Ellipse::Draw(impl::RenderData& ctx, const Entity& entity) {
	impl::DrawEllipse(ctx, entity);
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