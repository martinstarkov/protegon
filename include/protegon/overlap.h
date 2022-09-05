#pragma once

#include "vector2.h"
#include "rectangle.h"
#include "circle.h"

namespace ptgn {

namespace impl {

float SquareDistancePointRectangle(const Point<float>& a,
                                   const Rectangle<float>& b);

} // namespace impl

namespace overlap {

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 79.
bool RectangleRectangle(const Rectangle<float>& a,
					    const Rectangle<float>& b);

bool PointRectangle(const Point<float>& a,
					const Rectangle<float>& b);

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 88.
bool CircleCircle(const Circle<float>& a,
				  const Circle<float>& b);

bool PointCircle(const Point<float>& a,
				 const Circle<float>& b);

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 165-166.
bool CircleRectangle(const Circle<float>& a,
					 const Rectangle<float>& b);

/*
template <typename T, typename S = float,
	tt::floating_point<S> = true>
static bool LineAABB(const Line<T>& a,
					 const AABB<T>& b) {
	const Vector2<S> e{ b.size };
	const Vector2<S> d{ a.destination - a.origin };
	const Vector2<S> m{ a.origin + a.destination - 2 * b.position - b.size };
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
*/

// Source: https://www.jeffreythompson.org/collision-detection/line-circle.php
// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 179.
// Source: https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
// Source (used): https://www.baeldung.com/cs/circle-line-segment-collision-detection
/*
template <typename T>
static bool LineCircle(const Line<T>& a,
					   const Circle<T>& b) {
	// If the line is inside the circle entirely, exit early.
	if (PointCircle(a.origin, b) && PointCircle(a.destination, b)) return true;
	float min_dist2{ std::numeric_limits<S>::infinity() };
	const float rad2{ static_cast<float>(b.radius * b.radius) };
	// O is the circle center, P is the line origin, Q is the line destination.
	const V2_float OP{ a.origin - b.center };
	const V2_float OQ{ a.destination - b.center };
	const V2_float PQ{ a.destination - a.origin };
	const float OP_dist2{ OP.MagnitudeSquared() };
	const float OQ_dist2{ OQ.MagnitudeSquared() };
	const float max_dist2{ std::max(OP_dist2, OQ_dist2) };
	if (OP.Dot(-PQ) > 0 && OQ.Dot(PQ) > 0) {
		const float triangle_area{ math::FastAbs(math::ParallelogramArea(b.center, a.origin, a.destination)) / float{ 2 } };
		min_dist2 = 4 * triangle_area * triangle_area / PQ.MagnitudeSquared();
	} else {
		min_dist2 = std::min(OP_dist2, OQ_dist2);
	}
	return (min_dist2 < rad2 || NearlyEqual(min_dist2, rad2)) &&
		   (max_dist2 > rad2 || NearlyEqual(max_dist2, rad2));
}
*/


// Source: https://www.geeksforgeeks.org/check-if-two-given-line-segments-intersect/
// With some modifications.
/*
template <typename T>
static bool LineLine(const Line<T>& a,
					 const Line<T>& b) {
	// Sign of areas correspond to which side of ab points c and d are
	const T a1{ math::ParallelogramArea(a.origin, a.destination, b.destination) }; // Compute winding of abd (+ or -)
	const T a2{ math::ParallelogramArea(a.origin, a.destination, b.origin) }; // To intersect, must have sign opposite of a1
	// If c and d are on different sides of ab, areas have different signs
	bool polarity_diff{ false };
	bool collinear{ false };
	if constexpr (std::is_signed_v<T> && std::is_integral_v<T>) {
		// Second part for difference in polarity.
		polarity_diff = (a1 ^ a2) < 0;
		collinear = a1 == 0 || a2 == 0;
	} else {
		// Same as above but for floating points.
		polarity_diff = a1 * a2 < 0;
		collinear = NearlyEqual(a1, 0) || NearlyEqual(a2, 0);
	}
	if (!collinear && polarity_diff) {
		// Compute signs for a and b with respect to segment cd
		const T a3{ math::ParallelogramArea(b.origin, b.destination, a.origin) }; // Compute winding of cda (+ or -)
		// Since area is constant a1 - a2 = a3 - a4, or a4 = a3 + a2 - a1
		// const T a4 = math::ParallelogramArea(c, d, b); // Must have opposite sign of a3
		const T a4{ a3 + a2 - a1 };
		// Points a and b on different sides of cd if areas have different signs
		// Segments intersect if true.
		bool intersect{ false };
		// Check if a3 and a4 signs are different.
		if constexpr (std::is_signed_v<T> && std::is_integral_v<T>) {
			// If either is 0, the line is intersecting with the straight edge of the other line.
			// (i.e. corners with angles).
			intersect = (a3 ^ a4) < 0;
			collinear = a3 == 0 || a4 == 0;
		} else {
			// Same as above, hence the floating point comparison to 0.
			intersect = a3 * a4 < 0;
			collinear = NearlyEqual(a3, 0) || NearlyEqual(a4, 0);
		}
		if (intersect) return true;
	}
	return collinear &&
		  (math::PointLine(b.destination, a) ||
		   math::PointLine(b.origin, a)      ||
		   math::PointLine(a.origin, b)      ||
		   math::PointLine(a.destination, b));
}
*/

} // namespace overlap

} // namespace ptgn