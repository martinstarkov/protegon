#include "math/geometry/rect.h"

#include <array>

#include "common/assert.h"
#include "components/draw.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "math/geometry.h"
#include "math/vector2.h"
#include "renderer/api/origin.h"
#include "renderer/render_data.h"
#include "renderer/shader.h"
#include "scene/camera.h"

namespace ptgn {

Rect::Rect(const V2_float& min, const V2_float& max) : min{ min }, max{ max } {}

Rect::Rect(const V2_float& size) : min{ -size * 0.5f }, max{ size * 0.5f } {}

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

	ctx.AddQuad(transform, rect.GetSize(), origin, tint, depth, line_width, state);
}

V2_float Rect::GetSize() const {
	return max - min;
}

V2_float Rect::GetSize(const Transform& transform) const {
	return GetSize() * transform.scale;
}

Rect Rect::Offset(Origin origin) const {
	auto size{ GetSize() };
	auto offset{ GetOriginOffset(origin, size) };
	return Rect{ min - offset, max - offset };
}

std::array<V2_float, 4> Rect::GetWorldVertices(const Transform& transform) const {
	auto local_vertices{ GetLocalVertices() };
	return ToWorldPoint(local_vertices, transform);
}

std::array<V2_float, 4> Rect::GetLocalVertices() const {
	PTGN_ASSERT(min != max, "Cannot get local vertices for a rect with size zero");
	return { min, V2_float{ max.x, min.y }, max, V2_float{ min.x, max.y } };
}

std::array<V2_float, 4> Rect::GetWorldVertices(const Transform& transform, Origin origin) const {
	auto local_vertices{ GetLocalVertices(origin) };
	return ToWorldPoint(local_vertices, transform);
}

std::array<V2_float, 4> Rect::GetLocalVertices(Origin origin) const {
	auto rect{ Offset(origin) };
	return rect.GetLocalVertices();
}

V2_float Rect::GetCenter(const Transform& transform) const {
	return transform.position + (max + min) * 0.5f;
}

} // namespace ptgn