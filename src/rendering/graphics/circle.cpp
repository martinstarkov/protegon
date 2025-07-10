#include "rendering/graphics/circle.h"

#include <array>
#include <vector>

#include "algorithm"
#include "common/assert.h"
#include "components/draw.h"
#include "components/offsets.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "math/geometry.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "rendering/batching/render_data.h"
#include "rendering/resources/shader.h"
#include "scene/camera.h"
#include "scene/scene.h"

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

Entity CreateCircle(
	Scene& scene, const V2_float& position, float radius, const Color& color, float line_width
) {
	auto circle{ scene.CreateEntity() };

	circle.SetDraw<Circle>();
	circle.Show();

	circle.Add<Transform>(position);
	circle.Add<Circle>(radius);

	circle.SetTint(color);
	circle.Add<LineWidth>(line_width);

	return circle;
}

} // namespace ptgn