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
	PTGN_ASSERT(entity.Has<Circle>());

	auto depth{ entity.GetDepth() };
	auto blend_mode{ entity.GetBlendMode() };
	auto tint{ entity.GetTint() };
	auto origin{ entity.GetOrigin() };

	const auto& circle{ entity.Get<Circle>() };
	auto line_width{ entity.GetOrDefault<LineWidth>() };

	auto transform{ entity.GetDrawTransform() };

	auto camera{ entity.GetOrDefault<Camera>() };

	auto scaled_radius{ circle.radius * Abs(transform.scale) };

	// TODO: Make a zero scaled_size use cached camera vertices instead.
	PTGN_ASSERT(!scaled_radius.IsZero());

	V2_float scaled_size{ scaled_radius * 2.0f };

	// TODO: Cache everything from here onward.

	impl::RenderState render_state;

	render_state.blend_mode	   = blend_mode;
	render_state.shader_passes = entity.Get<impl::ShaderPass>();
	render_state.camera		   = camera;
	render_state.post_fx	   = entity.GetOrDefault<impl::PostFX>();

	if (line_width == -1.0f) {
		// Internally line width for a filled ellipse is 1.0f.
		line_width = 1.0f;
	} else {
		PTGN_ASSERT(line_width >= ctx.min_line_width, "Invalid line width for circle");

		// Internally line width for a completely hollow ellipse is 0.0f.
		// TODO: Check that dividing by std::max(scaled_radius.x, scaled_radius.y) does not cause
		// any unexpected bugs.
		line_width = 0.005f + line_width / std::min(scaled_radius.x, scaled_radius.y);
	}

	ctx.AddQuad(transform, scaled_size, origin, tint, depth, render_state, line_width);
}

Entity CreateCircle(
	Scene& scene, const V2_float& position, float radius, const Color& color, float line_width
) {
	auto circle{ scene.CreateEntity() };

	circle.SetDraw<Circle>();
	circle.Show();
	circle.Add<impl::ShaderPass>(game.shader.Get<ShapeShader::Circle>());

	circle.Add<Transform>(position);
	circle.Add<Circle>(radius);

	circle.SetTint(color);
	circle.Add<LineWidth>(line_width);

	return circle;
}

} // namespace ptgn