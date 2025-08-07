#include "math/geometry/circle.h"

#include "components/draw.h"
#include "components/transform.h"
#include "core/entity.h"
#include "math/vector2.h"
#include "renderer/render_data.h"

namespace ptgn {

Circle::Circle(float circle_radius) : radius{ circle_radius } {}

void Circle::Draw(impl::RenderData& ctx, const Entity& entity) {
	impl::DrawCircle(ctx, entity);
}

V2_float Circle::GetCenter(const Transform& transform) const {
	return transform.position;
}

float Circle::GetRadius() const {
	return radius;
}

float Circle::GetRadius(const Transform& transform) const {
	return GetRadius() * transform.GetAverageScale();
}

} // namespace ptgn