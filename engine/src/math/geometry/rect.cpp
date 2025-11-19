#include "math/geometry/rect.h"

#include <array>

#include "core/assert.h"
#include "ecs/components/origin.h"
#include "ecs/components/transform.h"
#include "math/vector2.h"

namespace ptgn {

Rect::Rect(const V2_float& min, const V2_float& max) : min{ min }, max{ max } {}

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
	return transform.Apply(local_vertices);
}

std::array<V2_float, 4> Rect::GetLocalVertices() const {
	PTGN_ASSERT(min != max, "Cannot get local vertices for a rect with size zero");
	return { min, V2_float{ max.x, min.y }, max, V2_float{ min.x, max.y } };
}

std::array<V2_float, 4> Rect::GetWorldVertices(const Transform& transform, Origin draw_origin)
	const {
	auto offset_transform{ Offset(transform, draw_origin) };
	auto local_vertices{ GetLocalVertices() };
	return offset_transform.Apply(local_vertices);
}

V2_float Rect::GetCenter(const Transform& transform) const {
	return transform.GetPosition() + (max + min) * 0.5f;
}

} // namespace ptgn