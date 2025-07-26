#include "rendering/graphics/rect.h"

#include <array>
#include <vector>

#include "common/assert.h"
#include "components/draw.h"
#include "components/offsets.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "debug/profiling.h"
#include "math/geometry.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "rendering/api/origin.h"
#include "rendering/api/vertex.h"
#include "rendering/render_data.h"
#include "rendering/resources/shader.h"
#include "scene/camera.h"
#include "scene/scene.h"

namespace ptgn {

Rect::Rect(const V2_float& rect_size) : size{ rect_size } {}

void Rect::Draw(impl::RenderData& ctx, const Entity& entity) {
	auto transform{ entity.GetDrawTransform() };
	PTGN_ASSERT(entity.Has<Rect>());
	const auto& rect{ entity.Get<Rect>() };
	auto origin{ entity.GetOrigin() };
	auto tint{ entity.GetTint() };
	auto depth{ entity.GetDepth() };
	auto line_width{ entity.GetOrDefault<LineWidth>() };

	impl::RenderState state;
	state.blend_mode  = entity.GetBlendMode();
	state.shader_pass = game.shader.Get<ShapeShader::Quad>();
	state.camera	  = entity.GetOrDefault<Camera>();
	state.post_fx	  = entity.GetOrDefault<impl::PostFX>();

	ctx.AddQuad(transform, rect.size, origin, tint, depth, line_width, state);
}

namespace impl {

Entity CreateRect(
	Manager& manager, const V2_float& position, const V2_float& size, const Color& color,
	float line_width, Origin origin
) {
	auto rect{ manager.CreateEntity() };

	rect.SetDraw<Rect>();
	rect.Show();

	rect.SetPosition(position);
	rect.Add<Rect>(size);
	rect.SetOrigin(origin);

	rect.SetTint(color);
	rect.Add<LineWidth>(line_width);

	return rect;
}

} // namespace impl

Entity CreateRect(
	Scene& scene, const V2_float& position, const V2_float& size, const Color& color,
	float line_width, Origin origin
) {
	return impl::CreateRect(scene, position, size, color, line_width, origin);
}

} // namespace ptgn