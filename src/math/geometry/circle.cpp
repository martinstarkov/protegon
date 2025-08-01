#include "math/geometry/circle.h"

#include "common/assert.h"
#include "components/draw.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "math/vector2.h"
#include "renderer/render_data.h"
#include "renderer/shader.h"
#include "scene/camera.h"

namespace ptgn {

Circle::Circle(float circle_radius) : radius{ circle_radius } {}

void Circle::Draw(impl::RenderData& ctx, const Entity& entity) {
	auto transform{ entity.GetDrawTransform() };
	PTGN_ASSERT(entity.Has<Circle>());
	const auto& circle{ entity.Get<Circle>() };
	auto tint{ entity.GetTint() };
	auto depth{ entity.GetDepth() };
	auto line_width{ entity.GetOrDefault<LineWidth>() };

	impl::RenderState state;
	state.blend_mode  = entity.GetBlendMode();
	state.shader_pass = game.shader.Get<ShapeShader::Circle>();
	state.camera	  = entity.GetOrDefault<Camera>();
	state.post_fx	  = entity.GetOrDefault<impl::PostFX>();

	ctx.AddCircle(transform, circle.radius, tint, depth, line_width, state);
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