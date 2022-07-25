#pragma once

#include "math/Vector2.h"

namespace ptgn {

namespace math {

// Returns 2 times the signed triangle area. The result is positive if
// abc is ccw, negative if abc is cw, zero if abc is degenerate.
template <typename T>
T SignedTriangleArea(const Vector2<T>& a, const Vector2<T>& b, const Vector2<T>& c) {
	return (a.x - c.x) * (b.y - c.y) - (a.y - c.y) * (b.x - c.x);
}

} // namespace math

namespace collision {

namespace overlap {

// Check if two lines overlap.
template <typename T>
inline bool LinevsLine(const math::Vector2<T>& line_origin,
					   const math::Vector2<T>& line_destination,
					   const math::Vector2<T>& other_line_origin,
					   const math::Vector2<T>& other_line_destination) {
	// Sign of areas correspond to which side of ab points c and d are
	T a1{ math::SignedTriangleArea(line_origin, line_destination, other_line_destination) }; // Compute winding of abd (+ or -)
	T a2{ math::SignedTriangleArea(line_origin, line_destination, other_line_origin) }; // To intersect, must have sign opposite of a1

	// If c and d are on different sides of ab, areas have different signs
	if (a1 * a2 < T(0)) {
		// Compute signs for a and b with respect to segment cd
		T a3{ math::SignedTriangleArea(other_line_origin, other_line_destination, line_origin) }; // Compute winding of cda (+ or -)
		// Since area is constant a1 - a2 = a3 - a4, or a4 = a3 + a2 - a1
		// float a4 = SignedTriangleArea(c, d, b); // Must have opposite sign of a3
		T a4{ a3 + a2 - a1 };
		// Points a and b on different sides of cd if areas have different signs
		// Segments intersect if true.
		return a3 * a4 < T(0);
	}
	// Segments not intersecting (or collinear)
	return false;
}

} // namespace overlap

} // namespace collision

} // namespace ptgn