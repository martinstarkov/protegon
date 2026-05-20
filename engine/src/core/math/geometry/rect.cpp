#include "core/math/geometry/rect.h"

#include <algorithm>
#include <array>
#include <span>

#include "core/assert.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"

namespace ptgn {

void Rect::SetSize(V2_float size) {
	*this = Rect{ size };
}

void Rect::SetSize(V2_float min, V2_float max) {
	min_ = min;
	max_ = max;
}

Rect Rect::Expanded(V2_float margin) const {
	return {
		min_ - margin,
		max_ + margin,
	};
}

V2_float Rect::GetSize() const {
	return max_ - min_;
}

V2_float Rect::GetMin() const {
	return min_;
}

V2_float Rect::GetMax() const {
	return max_;
}

V2_float& Rect::GetMin() {
	return min_;
}

V2_float& Rect::GetMax() {
	return max_;
}

V2_float Rect::GetSize(Transform transform) const {
	auto size{ GetSize() };
	auto scale{ transform.GetScale() };
	return size * Abs(scale);
}

Transform Rect::Offset(Transform transform, Origin draw_origin) const {
	auto size{ GetSize(transform) };
	auto offset{ GetOriginOffset(draw_origin, size) };
	if (offset.IsZero()) {
		return transform;
	}
	transform.Translate(-offset);
	return transform;
}

std::array<V2_float, 4> Rect::GetWorldVertices(Transform transform) const {
	auto local_vertices{ GetLocalVertices() };
	return transform.Apply(local_vertices);
}

std::array<V2_float, 4> Rect::GetLocalVertices() const {
	PTGN_ASSERT(min_ != max_, "Cannot get local vertices for a rect with size zero");
	return { min_, V2_float{ max_.x, min_.y }, max_, V2_float{ min_.x, max_.y } };
}

std::array<V2_float, 4> Rect::GetWorldVertices(Transform transform, Origin draw_origin) const {
	auto offset_transform{ Offset(transform, draw_origin) };
	auto local_vertices{ GetLocalVertices() };
	return offset_transform.Apply(local_vertices);
}

V2_float Rect::GetCenter(Transform transform) const {
	auto position{ transform.GetPosition() };
	auto center{ (max_ + min_) * 0.5f };
	return position + center;
}

Rect Rect::FromPoints(std::span<const V2_float> points) {
	PTGN_ASSERT(!points.empty(), "Cannot compute bounds of empty point list");

	auto bounds = Rect{
		points.front(),
		points.front(),
	};

	for (const auto& point : points) {
		bounds.min_.x = std::min(bounds.min_.x, point.x);
		bounds.min_.y = std::min(bounds.min_.y, point.y);
		bounds.max_.x = std::max(bounds.max_.x, point.x);
		bounds.max_.y = std::max(bounds.max_.y, point.y);
	}

	return bounds;
}

} // namespace ptgn