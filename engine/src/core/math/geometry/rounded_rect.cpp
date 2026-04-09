#include "core/math/geometry/rounded_rect.h"

#include <array>
#include <cstdlib>

#include "core/assert.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"

namespace ptgn {

void RoundedRect::SetRadius(float radius) {
	radius_ = radius;
}

void RoundedRect::SetSize(V2_float size) {
	min_ = -size * 0.5f;
	max_ = size * 0.5f;
}

void RoundedRect::SetSize(V2_float min, V2_float max) {
	min_ = min;
	max_ = max;
}

V2_float RoundedRect::GetSize() const {
	return max_ - min_;
}

float RoundedRect::GetRadius() const {
	return radius_;
}

V2_float RoundedRect::GetSize(Transform transform) const {
	auto size{ GetSize() };
	auto scale{ transform.GetScale() };
	return size * scale;
}

float RoundedRect::GetRadius(Transform transform) const {
	auto rounded_rect_radius{ GetRadius() };
	auto scale{ transform.GetAverageScale() };
	return rounded_rect_radius * std::abs(scale);
}

Transform RoundedRect::Offset(Transform transform, Origin draw_origin) const {
	auto size{ GetSize(transform) };
	auto offset{ GetOriginOffset(draw_origin, size) };
	if (offset.IsZero()) {
		return transform;
	}
	transform.Translate(-offset);
	return transform;
}

std::array<V2_float, 4> RoundedRect::GetWorldQuadVertices(Transform transform) const {
	auto local_vertices{ GetLocalQuadVertices() };
	return transform.Apply(local_vertices);
}

std::array<V2_float, 4> RoundedRect::GetLocalQuadVertices() const {
	PTGN_ASSERT(min_ != max_, "Cannot get local vertices for a rounded rect with size zero");
	return { min_, V2_float{ max_.x, min_.y }, max_, V2_float{ min_.x, max_.y } };
}

std::array<V2_float, 4> RoundedRect::GetWorldQuadVertices(Transform transform, Origin draw_origin)
	const {
	auto offset_transform{ Offset(transform, draw_origin) };
	auto local_vertices{ GetLocalQuadVertices() };
	return offset_transform.Apply(local_vertices);
}

V2_float RoundedRect::GetCenter(Transform transform) const {
	auto position{ transform.GetPosition() };
	auto center{ (max_ + min_) * 0.5f };
	return position + center;
}

} // namespace ptgn