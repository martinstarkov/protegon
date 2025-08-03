#include "math/geometry/line.h"

#include <array>

#include "common/assert.h"
#include "components/draw.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "math/geometry.h"
#include "math/geometry/rect.h"
#include "math/vector2.h"
#include "renderer/render_data.h"
#include "renderer/shader.h"
#include "scene/camera.h"

namespace ptgn {

Line::Line(const V2_float& start, const V2_float& end) : start{ start }, end{ end } {}

void Line::Draw(impl::RenderData& ctx, const Entity& entity) {
	auto transform{ entity.GetDrawTransform() };
	PTGN_ASSERT(entity.Has<Line>());
	const auto& line{ entity.Get<Line>() };
	auto tint{ entity.GetTint() };
	auto depth{ entity.GetDepth() };
	auto line_width{ entity.GetOrDefault<LineWidth>() };

	impl::RenderState state;
	state.blend_mode  = entity.GetBlendMode();
	state.shader_pass = game.shader.Get<ShapeShader::Quad>();
	state.camera	  = entity.GetOrDefault<Camera>();
	state.post_fx	  = entity.GetOrDefault<impl::PostFX>();

	auto [start, end] = line.GetWorldVertices(transform);

	ctx.AddLine(start, end, tint, depth, line_width, state);
}

std::array<V2_float, 4> Line::GetWorldQuadVertices(const Transform& transform, float line_width)
	const {
	auto dir{ end - start };

	//  TODO: Fix right and top side of line being 1 pixel thicker than left and bottom.
	auto local_center{ start + dir * 0.5f };

	V2_float center{ ToWorldPoint(local_center, transform) };

	float rotation{ dir.Angle() };
	V2_float size{ dir.Magnitude(), line_width };
	Rect rect{ size };
	return rect.GetWorldVertices(Transform{ center, rotation });
}

std::array<V2_float, 2> Line::GetWorldVertices(const Transform& transform) const {
	auto local_vertices{ GetLocalVertices() };
	return ToWorldPoint(local_vertices, transform);
}

std::array<V2_float, 2> Line::GetLocalVertices() const {
	return { start, end };
}

} // namespace ptgn