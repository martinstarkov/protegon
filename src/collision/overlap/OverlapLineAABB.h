#pragma once

#include "math/Vector2.h"
#include "math/Math.h"

namespace ptgn {


namespace math {

// Source: https://en.wikipedia.org/wiki/Cohen%E2%80%93Sutherland_algorithm

namespace cs {

typedef int OutCode;

const int INSIDE = 0; // 0000
const int LEFT = 1;   // 0001
const int RIGHT = 2;  // 0010
const int BOTTOM = 4; // 0100
const int TOP = 8;    // 1000

// Compute the bit code for a point (x, y) using the clip rectangle
// bounded diagonally by (xmin, ymin), and (xmax, ymax)

// ASSUME THAT xmax, xmin, ymax and ymin are global constants.

template <typename T>
OutCode ComputeOutCode(const T x, const T y, const Vector2<T>& min, const Vector2<T>& max) {
	OutCode code = INSIDE;  // initialised as being inside of clip window

	if (x < min.x)           // to the left of clip window
		code |= LEFT;
	else if (x > max.x)      // to the right of clip window
		code |= RIGHT;
	if (y < min.y)           // below the clip window
		code |= BOTTOM;
	else if (y > max.y)      // above the clip window
		code |= TOP;

	return code;
}

} // namespace cs

// Cohen–Sutherland clipping algorithm clips a line from
// P0 = (x0, y0) to P1 = (x1, y1) against a rectangle with 
// diagonal from (xmin, ymin) to (xmax, ymax).
template <typename T>
bool CohenSutherlandLineClip(T x0, T y0, T x1, T y1, const Vector2<T>& min, const Vector2<T>& max) {
	// compute outcodes for P0, P1, and whatever point lies outside the clip rectangle
	cs::OutCode outcode0 = cs::ComputeOutCode(x0, y0, min, max);
	cs::OutCode outcode1 = cs::ComputeOutCode(x1, y1, min, max);
	bool accept = false;

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
			// failed both tests, so calculate the line segment to clip
			// from an outside point to an intersection with clip edge
			double x, y;

			// At least one endpoint is outside the clip rectangle; pick it.
			cs::OutCode outcodeOut = outcode1 > outcode0 ? outcode1 : outcode0;

			// Now find the intersection point;
			// use formulas:
			//   slope = (y1 - y0) / (x1 - x0)
			//   x = x0 + (1 / slope) * (ym - y0), where ym is ymin or ymax
			//   y = y0 + slope * (xm - x0), where xm is xmin or xmax
			// No need to worry about divide-by-zero because, in each case, the
			// outcode bit being tested guarantees the denominator is non-zero
			if (outcodeOut & cs::TOP) {           // point is above the clip window
				x = x0 + (x1 - x0) * (max.y - y0) / (y1 - y0);
				y = max.y;
			} else if (outcodeOut & cs::BOTTOM) { // point is below the clip window
				x = x0 + (x1 - x0) * (min.y - y0) / (y1 - y0);
				y = min.y;
			} else if (outcodeOut & cs::RIGHT) {  // point is to the right of clip window
				y = y0 + (y1 - y0) * (max.x - x0) / (x1 - x0);
				x = max.x;
			} else if (outcodeOut & cs::LEFT) {   // point is to the left of clip window
				y = y0 + (y1 - y0) * (min.x - x0) / (x1 - x0);
				x = min.x;
			}

			// Now we move outside point to intersection point to clip
			// and get ready for next pass.
			if (outcodeOut == outcode0) {
				x0 = x;
				y0 = y;
				outcode0 = cs::ComputeOutCode(x0, y0, min, max);
			} else {
				x1 = x;
				y1 = y;
				outcode1 = cs::ComputeOutCode(x1, y1, min, max);
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
template <typename T>
inline bool LinevsAABB(const math::Vector2<T>& line_origin,
                       const math::Vector2<T>& line_destination,
                       const math::Vector2<T>& aabb_position,
                       const math::Vector2<T>& aabb_size) {
	return math::CohenSutherlandLineClip(line_origin.x, line_origin.y, line_destination.x, line_destination.y, aabb_position, aabb_position + aabb_size);
}

} // namespace overlap

} // namespace collision

} // namespace ptgn