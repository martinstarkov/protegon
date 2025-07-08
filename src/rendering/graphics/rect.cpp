#include "rendering/graphics/rect.h"

#include <array>
#include <vector>

#include "common/assert.h"
#include "components/draw.h"
#include "components/offsets.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "math/geometry.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "rendering/api/origin.h"
#include "rendering/batching/render_data.h"
#include "rendering/batching/vertex.h"
#include "rendering/resources/shader.h"
#include "scene/camera.h"
#include "scene/scene.h"

namespace ptgn {

Rect::Rect(const V2_float& rect_size) : size{ rect_size } {}

void Rect::Draw(impl::RenderData& ctx, const Entity& entity) {
	PTGN_ASSERT(entity.Has<Rect>());

	auto depth{ entity.GetDepth() };
	auto blend_mode{ entity.GetBlendMode() };
	auto tint{ entity.GetTint() };
	auto origin{ entity.GetOrigin() };

	const auto& rect{ entity.Get<Rect>() };
	auto line_width{ entity.GetOrDefault<LineWidth>() };

	auto transform{ entity.GetDrawTransform() };

	auto camera{ entity.GetOrDefault<Camera>() };

	auto scaled_size{ rect.size * Abs(transform.scale) };

	// TODO: Make a zero scaled_size use cached camera vertices instead.
	PTGN_ASSERT(!scaled_size.IsZero());

	// TODO: Cache everything from here onward.

	impl::RenderState render_state;

	render_state.blend_mode	 = blend_mode;
	render_state.shader_pass = entity.Get<impl::ShaderPass>();
	render_state.camera		 = camera;
	render_state.post_fx	 = entity.GetOrDefault<impl::PostFX>();

	if (line_width == -1.0f) {
		ctx.AddQuad(transform, scaled_size, origin, tint, depth, render_state);
		return;
	}

	// TODO: Move into renderer.

	ctx.AddThinQuad(transform, scaled_size, origin, tint, depth, line_width, render_state);
}

Entity CreateRect(
	Scene& scene, const V2_float& position, const V2_float& size, const Color& color,
	float line_width, Origin origin
) {
	auto rect{ scene.CreateEntity() };

	rect.SetDraw<Rect>();
	rect.Show();
	rect.Add<impl::ShaderPass>(game.shader.Get<ShapeShader::Quad>());

	rect.Add<Transform>(position);
	rect.Add<Rect>(size);
	rect.SetOrigin(origin);

	rect.SetTint(color);
	rect.Add<LineWidth>(line_width);

	return rect;
}

} // namespace ptgn