#include "math/geometry/rect.h"

#include <array>

#include "common/assert.h"
#include "components/draw.h"
#include "components/transform.h"
#include "core/entity.h"
#include "math/geometry.h"
#include "math/vector2.h"
#include "rect.h"
#include "renderer/api/origin.h"
#include "renderer/render_data.h"

namespace ptgn {

Rect::Rect(const V2_float& min, const V2_float& max) : min{ min }, max{ max } {}

Rect::Rect(const V2_float& size) : min{ -size * 0.5f }, max{ size * 0.5f } {}

void Rect::Draw(impl::RenderData& ctx, const Entity& entity) {
	impl::DrawRect(ctx, entity);
}

V2_float Rect::GetSize() const {
	return max - min;
}

V2_float Rect::GetSize(const Transform& transform) const {
	return GetSize() * Abs(transform.GetScale());
}

Transform Rect::Offset(const Transform& transform, Origin draw_origin) const {
	auto offset{ GetOriginOffset(draw_origin, GetSize(transform)) };
	if (offset.IsZero()) {
		return transform;
	}
	Transform result{ transform };
	result.Translate(-offset);
	return result;
}

std::array<V2_float, 4> Rect::GetWorldVertices(const Transform& transform) const {
	auto local_vertices{ GetLocalVertices() };
	return ApplyTransform(local_vertices, transform);
}

std::array<V2_float, 4> Rect::GetLocalVertices() const {
	PTGN_ASSERT(min != max, "Cannot get local vertices for a rect with size zero");
	return { min, V2_float{ max.x, min.y }, max, V2_float{ min.x, max.y } };
}

std::array<V2_float, 4> Rect::GetWorldVertices(const Transform& transform, Origin draw_origin)
	const {
	auto offset{ Offset(transform, draw_origin) };
	auto local_vertices{ GetLocalVertices() };
	return ApplyTransform(local_vertices, offset);
}

V2_float Rect::GetCenter(const Transform& transform) const {
	return transform.GetPosition() + (max + min) * 0.5f;
}

} // namespace ptgn