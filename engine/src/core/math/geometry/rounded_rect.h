#pragma once

#include <cstdlib>

#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "serialization/serialize.h"

namespace ptgn {

/// @brief RoundedRect has no rotation center because this can be achieved via using a parent Entity
/// and positioning it where the origin should be.
class RoundedRect {
public:
	Rect rect;
	float radius{ 0.0f };

	constexpr RoundedRect() = default;

	constexpr RoundedRect(V2_float min, V2_float max, float rounded_rect_radius) :
		rect{ min, max }, radius{ rounded_rect_radius } {}

	constexpr RoundedRect(V2_float size, float rounded_rect_radius) :
		RoundedRect{ -size * 0.5f, size * 0.5f, rounded_rect_radius } {}

	constexpr float GetRadius(Transform transform) const {
		auto scale{ transform.GetAverageScale() };
		return radius * std::abs(scale);
	}

	constexpr bool operator==(const RoundedRect&) const = default;

	PTGN_REFLECT(RoundedRect, rect, radius)
};

} // namespace ptgn