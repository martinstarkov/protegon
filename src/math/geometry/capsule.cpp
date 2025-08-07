#include "math/geometry/capsule.h"

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

Capsule::Capsule(const V2_float& start, const V2_float& end, float radius) :
	start{ start }, end{ end }, radius{ radius } {}

void Capsule::Draw(impl::RenderData& ctx, const Entity& entity) {
	auto transform{ GetDrawTransform(entity) };
	PTGN_ASSERT(entity.Has<Capsule>());
	const auto& capsule{ entity.Get<Capsule>() };
	auto tint{ entity.GetTint() };
	auto depth{ entity.GetDepth() };
	auto line_width{ entity.GetOrDefault<LineWidth>() };

	impl::RenderState state;
	state.blend_mode  = entity.GetBlendMode();
	state.shader_pass = game.shader.Get<ShapeShader::Quad>();
	state.camera	  = entity.GetOrDefault<Camera>();
	state.post_fx	  = entity.GetOrDefault<impl::PostFX>();

	auto [start, end] = capsule.GetWorldVertices(transform);

	// TODO: Replace with arcs.
	ctx.AddCircle(Transform{ start }, capsule.radius, tint, depth, line_width, state);
	ctx.AddCircle(Transform{ end }, capsule.radius, tint, depth, line_width, state);

	// TODO: Replace with two lines connecting arcs.
	ctx.AddLine(start, end, tint, depth, line_width, state);
}

std::array<V2_float, 2> Capsule::GetWorldVertices(const Transform& transform) const {
	auto local_vertices{ GetLocalVertices() };
	return ToWorldPoint(local_vertices, transform);
}

std::array<V2_float, 2> Capsule::GetLocalVertices() const {
	return { start, end };
}

float Capsule::GetRadius() const {
	return radius;
}

float Capsule::GetRadius(const Transform& transform) const {
	return GetRadius() * transform.GetAverageScale();
}

} // namespace ptgn