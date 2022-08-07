#pragma once

#include "math/Vector2.h"
#include "math/Math.h"
#include "utility/TypeTraits.h"
#include "collision/Types.h"
#include "collision/overlap/OverlapPoint.h"
#include "collision/overlap/OverlapCapsule.h"

namespace ptgn {

namespace math {

// TODO: This is essentially the same as SignedTriangleArea
// Get the area of the triangle formed by points a, b, c.
template <typename T, typename S = double,
	tt::floating_point<S> = true>
inline S TriangleArea(const math::Vector2<T>& a,
					  const math::Vector2<T>& b,
					  const math::Vector2<T>& c) {
	const math::Vector2<S> ab{ b - a };
	const math::Vector2<S> ac{ c - a };
	return math::FastAbs(ab.Cross(ac)) / 2;
}

// Returns 2 times the signed triangle area. The result is positive if
// abc is ccw, negative if abc is cw, zero if abc is degenerate.
template <typename T>
inline T SignedTriangleArea(const Vector2<T>& a,
							const Vector2<T>& b,
							const Vector2<T>& c) {
	return (a.x - c.x) * (b.y - c.y) - (a.y - c.y) * (b.x - c.x);
}


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
	tt::floating_point<T> = true>
OutCode ComputeOutCode(const math::Vector2<T>& p,
					   const math::Vector2<T>& min,
					   const math::Vector2<T>& max) {
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
	tt::floating_point<T> = true>
bool CohenSutherlandLineClip(math::Vector2<T> p0,
							 math::Vector2<T> p1,
						     const math::Vector2<T>& min,
							 const math::Vector2<T>& max) {
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

namespace overlap {

// Check if a line and an AABB overlap.
// AABB position is taken from top left.
// AABB size is the full extent from top left to bottom right.
template <typename T, typename S = double,
	tt::floating_point<S> = true>
static bool LineAABB(const Line<T>& a,
					 const AABB<T>& b) {
	const math::Vector2<S> e{ b.size };
	const math::Vector2<S> d{ a.destination - a.origin };
	const math::Vector2<S> m{ a.origin + a.destination - 2 * b.position - b.size };
	// Try world coordinate axes as separating axes
	S adx{ math::FastAbs(d.x) };
	if (math::FastAbs(m.x) > e.x + adx) return false;
	S ady{ math::FastAbs(d.y) };
	if (math::FastAbs(m.y) > e.y + ady) return false;
	// Add in an epsilon term to counteract arithmetic errors when segment is
	// (near) parallel to a coordinate axis (see text for detail)
	adx += math::epsilon<S>;
	ady += math::epsilon<S>;
	// Try cross products of segment direction vector with coordinate axes
	if (math::FastAbs(m.x * d.y - m.y * d.x) > e.x * ady + e.y * adx) return false;
	// No separating axis found; segment must be overlapping AABB
	return true;

	// Alternative method:
	// Source: https://en.wikipedia.org/wiki/Cohen%E2%80%93Sutherland_algorithm
	//return math::CohenSutherlandLineClip(a.origin, a.destination, b.Min(), b.Max());
}

// Check if a line and a capsule overlap.
// Capsule origin and destination are taken from the edge circle centers.
template <typename T, typename S = double,
	tt::floating_point<S> = true>
inline bool LineCapsule(const Line<T>& a,
						const Capsule<T>& b) {
	return CapsuleCapsule({ a.origin, a.destination, T{ 0 } }, b);
}

// Source: https://www.jeffreythompson.org/collision-detection/line-circle.php
// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 179.
// Source: https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
// Source (used): https://www.baeldung.com/cs/circle-line-segment-collision-detection
// Check if a line and a circle overlap.
// Circle position is taken from its center.
template <typename T, typename S = double,
	tt::floating_point<S> = true>
static bool LineCircle(const Line<T>& a,
					   const Circle<T>& b) {
	static_assert(!tt::is_narrowing_v<T, S>);
	// If the line is inside the circle entirely, exit early.
	if (PointCircle(Point{ a.origin }, b) && PointCircle(Point{ a.destination }, b)) return true;
	S min_dist2{ std::numeric_limits<S>::infinity() };
	const S rad2{ static_cast<S>(b.RadiusSquared()) };
	// O is the circle center, P is the line origin, Q is the line destination.
	const math::Vector2<S> OP{ a.origin - b.center };
	const math::Vector2<S> OQ{ a.destination - b.center };
	const math::Vector2<S> PQ{ a.destination - a.origin };
	const S OP_dist2{ OP.MagnitudeSquared() };
	const S OQ_dist2{ OQ.MagnitudeSquared() };
	const S max_dist2{ std::max(OP_dist2, OQ_dist2) };
	if (OP.Dot(-PQ) > 0 && OQ.Dot(PQ) > 0) {
		const S triangle_area{ math::TriangleArea<S>(b.center, a.origin, a.destination) };
		min_dist2 = 4 * triangle_area * triangle_area / PQ.MagnitudeSquared();
	} else {
		min_dist2 = std::min(OP_dist2, OQ_dist2);
	}
	return (min_dist2 < rad2 || math::Compare(min_dist2, rad2)) &&
		   (max_dist2 > rad2 || math::Compare(max_dist2, rad2));
}

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Source: Page 152-153 with modifications for collinearity and straight edge intersections.
// Check if two lines overlap.
template <typename T>
static bool LineLine(const Line<T>& a,
					 const Line<T>& b) {
	// Sign of areas correspond to which side of ab points c and d are
	const T a1{ math::SignedTriangleArea(a.origin, a.destination, b.destination) }; // Compute winding of abd (+ or -)
	const T a2{ math::SignedTriangleArea(a.origin, a.destination, b.origin) }; // To intersect, must have sign opposite of a1
	// If c and d are on different sides of ab, areas have different signs
	bool different_sides{ false };
	// Check if a1 and a2 signs are different.
	bool collinear{ false };
	if constexpr (std::is_signed_v<T> && std::is_integral_v<T>) {
		// First part checks for collinearity, second part for difference in polarity.
		collinear = !((a1 | a2) != 0);
		different_sides = !collinear && (a1 ^ a2) < 0;
	} else {
		// Same as above but for floating points.
		collinear = math::Compare(a1, 0) || math::Compare(a2, 0);
		different_sides = !collinear && a1 * a2 < 0;
	}
	if (different_sides) {
		// Compute signs for a and b with respect to segment cd
		const T a3{ math::SignedTriangleArea(b.origin, b.destination, a.origin) }; // Compute winding of cda (+ or -)
		// Since area is constant a1 - a2 = a3 - a4, or a4 = a3 + a2 - a1
		// const T a4 = SignedTriangleArea(c, d, b); // Must have opposite sign of a3
		const T a4{ a3 + a2 - a1 };
		// Points a and b on different sides of cd if areas have different signs
		// Segments intersect if true.
		bool intersect{ false };
		// Check if a3 and a4 signs are different.
		if constexpr (std::is_signed_v<T> && std::is_integral_v<T>) {
			// If either is 0, the line is intersecting with the straight edge of the other line.
			// (i.e. corners with angles).
			intersect = a3 == 0 || a4 == 0 || (a3 ^ a4) < 0;
		} else {
			// Same as above, hence the floating point comparison to 0.
			const T result{ a3 * a4 };
			intersect = result < 0 || math::Compare(result, 0);
		}
		return intersect;
	}
	if (collinear) {
		return PointLine(Point{ a.origin }, b) ||
			   PointLine(Point{ a.destination }, b) ||
			   PointLine(Point{ b.origin }, a) ||
			   PointLine(Point{ b.destination }, a);
	}
	// Segments not intersecting.
	return false;
}

} // namespace overlap

} // namespace ptgn