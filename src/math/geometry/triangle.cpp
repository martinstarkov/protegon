
#include "math/geometry/triangle.h"

#include <array>

#include "common/assert.h"
#include "components/draw.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "math/geometry.h"
#include "math/vector2.h"
#include "renderer/render_data.h"
#include "renderer/shader.h"
#include "scene/camera.h"

namespace ptgn {

Triangle::Triangle(const V2_float& a, const V2_float& b, const V2_float& c) :
	a{ a }, b{ b }, c{ c } {}

Triangle::Triangle(const std::array<V2_float, 3>& vertices) :
	a{ vertices[0] }, b{ vertices[1] }, c{ vertices[2] } {}

void Triangle::Draw(impl::RenderData& ctx, const Entity& entity) {
	auto transform{ entity.GetDrawTransform() };
	PTGN_ASSERT(entity.Has<Triangle>());
	const auto& triangle{ entity.Get<Triangle>() };
	auto tint{ entity.GetTint() };
	auto depth{ entity.GetDepth() };
	auto line_width{ entity.GetOrDefault<LineWidth>() };

	impl::RenderState state;
	state.blend_mode  = entity.GetBlendMode();
	state.shader_pass = game.shader.Get<ShapeShader::Quad>();
	state.camera	  = entity.GetOrDefault<Camera>();
	state.post_fx	  = entity.GetOrDefault<impl::PostFX>();

	auto points{ triangle.GetWorldVertices(transform) };

	ctx.AddTriangle(points, tint, depth, line_width, state);
}

std::array<V2_float, 3> Triangle::GetWorldVertices(const Transform& transform) const {
	auto local_vertices{ GetLocalVertices() };
	return ToWorldPoint(local_vertices, transform);
}

std::array<V2_float, 3> Triangle::GetLocalVertices() const {
	return { a, b, c };
}

} // namespace ptgn