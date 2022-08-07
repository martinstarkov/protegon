#pragma once

#include <type_traits> // std::is_signed_v, ...

#include "math/Vector2.h"
#include "math/Math.h"
#include "collision/overlap/OverlapPointLine.h"

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Source: Page 152-153 with modifications for collinearity and straight edge intersections.

namespace ptgn {

namespace math {

// Returns 2 times the signed triangle area. The result is positive if
// abc is ccw, negative if abc is cw, zero if abc is degenerate.
template <typename T>
const T SignedTriangleArea(const Vector2<T>& a, const Vector2<T>& b, const Vector2<T>& c) {
	return (a.x - c.x) * (b.y - c.y) - (a.y - c.y) * (b.x - c.x);
}

} // namespace math

namespace collision {

namespace overlap {

// Check if two lines overlap.
template <typename T>
static bool LinevsLine(const math::Vector2<T>& line_origin,
					   const math::Vector2<T>& line_destination,
					   const math::Vector2<T>& other_line_origin,
					   const math::Vector2<T>& other_line_destination) {
	// Sign of areas correspond to which side of ab points c and d are
	const T a1{ math::SignedTriangleArea(line_origin, line_destination, other_line_destination) }; // Compute winding of abd (+ or -)
	const T a2{ math::SignedTriangleArea(line_origin, line_destination, other_line_origin) }; // To intersect, must have sign opposite of a1
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
		const T a3{ math::SignedTriangleArea(other_line_origin, other_line_destination, line_origin) }; // Compute winding of cda (+ or -)
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
		return PointLine(line_origin, other_line_origin, other_line_destination) || 
			   PointLine(line_destination, other_line_origin, other_line_destination) ||
			   PointLine(other_line_origin, line_origin, line_destination) ||
			   PointLine(other_line_destination, line_origin, line_destination);
	}
	// Segments not intersecting.
	return false;
}

} // namespace overlap

} // namespace collision

} // namespace ptgn