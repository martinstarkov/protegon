#pragma once

#include "math/Vector2.h"
#include "math/Math.h"
#include "collision/overlap/OverlapCircleCircle.h"

// Source: https://steamcdn-a.akamaihd.net/apps/valve/2015/DirkGregorius_Contacts.pdf

namespace ptgn {

namespace collision {

namespace overlap {

// Check if a point and a circle overlap.
// Circle position is taken from its center.
template <typename T>
inline bool PointvsCircle(const math::Vector2<T>& point,
						  const math::Vector2<T>& circle_position,
						  const T circle_radius) {
	return CirclevsCircle(point, static_cast<T>(0), circle_position, circle_radius);
}

} // namespace overlap

} // namespace collision

} // namespace ptgn