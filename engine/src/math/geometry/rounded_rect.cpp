#include "math/geometry/rounded_rect.h"

#include <array>

#include "core/assert.h"
#include "ecs/components/draw.h"
#include "ecs/components/transform.h"
#include "ecs/entity.h"
#include "math/math_utils.h"
#include "math/vector2.h"
#include "renderer/api/origin.h"

namespace ptgn {

RoundedRect::RoundedRect(const V2_float& min, const V2_float& max, float radius) :
	min{ min }, max{ max }, radius{ radius } {}

RoundedRect::RoundedRect(const V2_float& size, float radius) :
	min{ -size * 0.5f }, max{ size * 0.5f }, radius{ radius } {}

void RoundedRect::Draw(const Entity& entity) {
	// TODO: Fix.
	// impl::DrawRoundedRect(entity);
}

V2_float RoundedRect::GetSize() const {
	return max - min;
}

float RoundedRect::GetRadius() const {
	return radius;
}

V2_float RoundedRect::GetSize(const Transform& transform) const {
	return GetSize() * Abs(transform.GetScale());
}

float RoundedRect::GetRadius(const Transform& transform) const {
	return GetRadius() * Abs(transform.GetAverageScale());
}

Transform RoundedRect::Offset(const Transform& transform, Origin draw_origin) const {
	auto offset{ GetOriginOffset(draw_origin, GetSize(transform)) };
	if (offset.IsZero()) {
		return transform;
	}
	Transform result{ transform };
	result.Translate(-offset);
	return result;
}

std::array<V2_float, 4> RoundedRect::GetWorldQuadVertices(const Transform& transform) const {
	auto local_vertices{ GetLocalQuadVertices() };
	return transform.Apply(local_vertices);
}

std::array<V2_float, 4> RoundedRect::GetLocalQuadVertices() const {
	PTGN_ASSERT(min != max, "Cannot get local vertices for a rounded rect with size zero");
	return { min, V2_float{ max.x, min.y }, max, V2_float{ min.x, max.y } };
}

std::array<V2_float, 4> RoundedRect::GetWorldQuadVertices(
	const Transform& transform, Origin draw_origin
) const {
	auto offset_transform{ Offset(transform, draw_origin) };
	auto local_vertices{ GetLocalQuadVertices() };
	return offset_transform.Apply(local_vertices);
}

V2_float RoundedRect::GetCenter(const Transform& transform) const {
	return transform.GetPosition() + (max + min) * 0.5f;
}

} // namespace ptgn