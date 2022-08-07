#pragma once

#include "collision/Types.h"

namespace ptgn {

namespace overlap {

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 79.
template <typename T>
inline bool AABBAABB(const AABB<T>& a, const AABB<T>& b) {
	if (a.position.x + a.size.x < b.position.x || a.position.x > b.position.x + b.size.x) return false;
	if (a.position.y + a.size.y < b.position.y || a.position.y > b.position.y + b.size.y) return false;
	return true;
}

} // namespace overlap

} // namespace ptgn