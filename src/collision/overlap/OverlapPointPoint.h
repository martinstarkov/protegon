#pragma once

#include "math/Vector2.h"

namespace ptgn {

namespace collision {

namespace overlap {

template <typename T>
inline bool PointPoint(const math::Vector2<T>& point,
					   const math::Vector2<T>& other_point) {
	return point == other_point;
}

}

}

}