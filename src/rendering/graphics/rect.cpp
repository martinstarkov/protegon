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

	std::array<V2_float, 4> quad_points{ impl::GetVertices(transform, scaled_size, origin) };

	auto quad_vertices{
		impl::GetQuadVertices(quad_points, tint, depth, 0.0f, impl::default_texture_coordinates)
	};

	impl::RenderState render_state;

	render_state.blend_mode	   = blend_mode;
	render_state.shader_passes = entity.Get<impl::ShaderPass>();
	render_state.camera		   = camera;
	render_state.post_fx	   = entity.GetOrDefault<impl::PostFX>();
	render_state.pre_fx		   = entity.GetOrDefault<impl::PreFX>();

	if (line_width == -1.0f) {
		ctx.AddQuad(quad_vertices, render_state);
		return;
	}

	PTGN_ASSERT(line_width >= ctx.min_line_width, "Invalid line width for Rect");

	// Compose the quad out of 4 lines, each made up of line_width wide quads.
	for (std::size_t i{ 0 }; i < quad_points.size(); i++) {
		auto start{ camera.ZoomIfNeeded(quad_points[i]) };
		auto end{ camera.ZoomIfNeeded(quad_points[(i + 1) % quad_points.size()]) };

		auto line_points{ impl::GetLineQuadVertices(start, end, line_width) };

		for (std::size_t j{ 0 }; j < line_points.size(); j++) {
			quad_vertices[j].position[0] = line_points[j].x;
			quad_vertices[j].position[1] = line_points[j].y;
		}

		ctx.AddQuad(quad_vertices, render_state);
	}
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