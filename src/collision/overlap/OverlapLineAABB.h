#pragma once

#include <type_traits> // std::enable_if_t, ...

#include "math/Vector2.h"
#include "math/Math.h"

// Source: https://en.wikipedia.org/wiki/Cohen%E2%80%93Sutherland_algorithm

namespace ptgn {

namespace math {

namespace cs {

typedef int OutCode;

constexpr const int INSIDE = 0; // 0000
constexpr const int LEFT = 1;   // 0001
constexpr const int RIGHT = 2;  // 0010
constexpr const int BOTTOM = 4; // 0100
constexpr const int TOP = 8;    // 1000

// Compute the bit code for a point p (x, y) using the clip rectangle
// bounded diagonally by (xmin, ymin), and (xmax, ymax)

// ASSUME THAT xmax, xmin, ymax and ymin are global constants.

template <typename T = double,
	std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
const OutCode ComputeOutCode(const math::Vector2<T>& p, const math::Vector2<T>& min, const math::Vector2<T>& max) {
	OutCode code = INSIDE;  // initialised as being inside of clip window

	if (p.x < min.x)           // to the left of clip window
		code |= LEFT;
	else if (p.x > max.x)      // to the right of clip window
		code |= RIGHT;
	
	if (p.y < min.y)           // below the clip window
		code |= BOTTOM;
	else if (p.y > max.y)      // above the clip window
		code |= TOP;

	return code;
}

} // namespace cs

// Cohen–Sutherland clipping algorithm clips a line from
// p0 = (x0, y0) to p1 = (x1, y1) against a rectangle with 
// diagonal from (xmin, ymin) to (xmax, ymax).
template <typename T = double,
	std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
bool CohenSutherlandLineClip(math::Vector2<T> p0, math::Vector2<T> p1,
							 const math::Vector2<T>& min, const math::Vector2<T>& max) {
	// compute outcodes for P0, P1, and whatever point lies outside the clip rectangle
	cs::OutCode outcode0{ cs::ComputeOutCode<T>(p0, min, max) };
	cs::OutCode outcode1{ cs::ComputeOutCode<T>(p1, min, max) };
	bool accept{ false };

	while (true) {
		if (!(outcode0 | outcode1)) {
			// bitwise OR is 0: both points inside window; trivially accept and exit loop
			accept = true;
			break;
		} else if (outcode0 & outcode1) {
			// bitwise AND is not 0: both points share an outside zone (LEFT, RIGHT, TOP,
			// or BOTTOM), so both must be outside window; exit loop (accept is false)
			break;
		} else {
			math::Vector2<T> p;
			// At least one endpoint is outside the clip rectangle; pick it.
			const cs::OutCode outcodeOut{ outcode1 > outcode0 ? outcode1 : outcode0 };

			// Now find the intersection point;
			// use formulas:
			//   slope = (y1 - y0) / (x1 - x0)
			//   x = x0 + (1 / slope) * (ym - y0), where ym is ymin or ymax
			//   y = y0 + slope * (xm - x0), where xm is xmin or xmax
			// No need to worry about divide-by-zero because, in each case, the
			// outcode bit being tested guarantees the denominator is non-zero
			if (outcodeOut & cs::TOP) {           // point is above the clip window
				p.x = p0.x + (p1.x - p0.x) * (max.y - p0.y) / (p1.y - p0.y);
				p.y = max.y;
			} else if (outcodeOut & cs::BOTTOM) { // point is below the clip window
				p.x = p0.x + (p1.x - p0.x) * (min.y - p0.y) / (p1.y - p0.y);
				p.y = min.y;
			} else if (outcodeOut & cs::RIGHT) {  // point is to the right of clip window
				p.y = p0.y + (p1.y - p0.y) * (max.x - p0.x) / (p1.x - p0.x);
				p.x = max.x;
			} else if (outcodeOut & cs::LEFT) {   // point is to the left of clip window
				p.y = p0.y + (p1.y - p0.y) * (min.x - p0.x) / (p1.x - p0.x);
				p.x = min.x;
			}
			// Now we move outside point to intersection point to clip
			// and get ready for next pass.
			if (outcodeOut == outcode0) {
				p0.x = p.x;
				p0.y = p.y;
				outcode0 = cs::ComputeOutCode<T>(p0, min, max);
			} else {
				p1.x = p.x;
				p1.y = p.y;
				outcode1 = cs::ComputeOutCode<T>(p1, min, max);
			}
		}
	}
	return accept;
}

} // namespace math

namespace collision {

namespace overlap {

// Check if a line and an AABB overlap.
// AABB position is taken from top left.
// AABB size is the full extent from top left to bottom right.
template <typename T, typename S = double,
	std::enable_if_t<std::is_floating_point_v<S>, bool> = true>
static bool LinevsAABB(const math::Vector2<T>& line_origin,
                       const math::Vector2<T>& line_destination,
                       const math::Vector2<T>& aabb_position,
                       const math::Vector2<T>& aabb_size) {
	const math::Vector2<S> e{ aabb_position + aabb_size - aabb_position };
	const math::Vector2<S> d{ line_destination - line_origin };
	const math::Vector2<S> m{ line_origin + line_destination - 2 * aabb_position - aabb_size };
	// Try world coordinate axes as separating axes
	S adx{ math::Abs(d.x) };
	if (math::Abs(m.x) > e.x + adx) return false;
	S ady{ math::Abs(d.y) };
	if (math::Abs(m.y) > e.y + ady) return false;
	// Add in an epsilon term to counteract arithmetic errors when segment is
	// (near) parallel to a coordinate axis (see text for detail)
	adx += math::EPSILON<S>;
	ady += math::EPSILON<S>;
	// Try cross products of segment direction vector with coordinate axes
	if (math::Abs(m.x * d.y - m.y * d.x) > e.x * ady + e.y * adx) return false;
	// No separating axis found; segment must be overlapping AABB
	return true;

	// Alternative method:
	//return math::CohenSutherlandLineClip(line_origin, line_destination, aabb_position, aabb_position + aabb_size);
}

} // namespace overlap

} // namespace collision

} // namespace ptgn