#include "math/geometry/line.h"

#include "math/geometry/circle.h"
#include "math/geometry/polygon.h"
#include "math/math.h"
#include "math/utility.h"
#include "math/vector2.h"

namespace ptgn {

V2_float Line::Direction() const {
	return b - a;
}

V2_float Line::Midpoint() const {
	return (a + b) * 0.5f;
}

bool Line::Overlaps(const V2_float& point) const {
	// Source:
	// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 130. (SqDistPointSegment == 0) but optimized.

	V2_float ab{ Direction() };
	V2_float ac{ point - a };
	V2_float bc{ point - b };

	float e{ ac.Dot(ab) };
	// Handle cases where c projects outside ab.
	if (e < 0 || NearlyEqual(e, 0.0f)) {
		return NearlyEqual(ac.x, 0.0f) && NearlyEqual(ac.y, 0.0f);
	}

	float f{ ab.Dot(ab) };
	if (e > f || NearlyEqual(e, f)) {
		return NearlyEqual(bc.x, 0.0f) && NearlyEqual(bc.y, 0.0f);
	}

	// Handle cases where c projects onto ab.
	return NearlyEqual(ac.Dot(ac) * f, e * e);
}

bool Line::Overlaps(const Line& line) const {
	// Source:
	// https://www.geeksforgeeks.org/check-if-two-given-line-segments-intersect/

	// Sign of areas correspond to which side of ab points c and d are
	float a1{ impl::ParallelogramArea(a, b, line.b) }; // Compute winding of abd (+ or -)
	float a2{
		impl::ParallelogramArea(a, b, line.a)
	}; // To intersect, must have sign opposite of a1
	// If c and d are on different sides of ab, areas have different signs
	bool polarity_diff{ false };
	bool collinear{ false };
	// Same as above but for floating points.
	polarity_diff = a1 * a2 < 0.0f;
	collinear	  = NearlyEqual(a1, 0.0f) || NearlyEqual(a2, 0.0f);
	// For integral implementation use this instead of the above two lines:
	// if constexpr (std::is_signed_v<T> && std::is_integral_v<T>) {
	//	// Second part for difference in polarity.
	//	polarity_diff = (a1 ^ a2) < 0;
	//	collinear = a1 == 0 || a2 == 0;
	//}
	if (!collinear && polarity_diff) {
		// Compute signs for a and b with respect to segment cd
		float a3{ impl::ParallelogramArea(line.a, line.b, a) }; // Compute winding of cda (+ or -)
		// Since area is constant a1 - a2 = a3 - a4, or a4 = a3 + a2 - a1
		// const T a4 = math::ParallelogramArea(c, d, b); // Must have opposite
		// sign of a3
		float a4{ a3 + a2 - a1 };
		// Points a and b on different sides of cd if areas have different signs
		// Segments intersect if true.
		bool intersect{ false };
		// If either is 0, the line is intersecting with the straight edge of
		// the other line. (i.e. corners with angles). Check if a3 and a4 signs
		// are different.
		intersect = a3 * a4 < 0.0f;
		collinear = NearlyEqual(a3, 0.0f) || NearlyEqual(a4, 0.0f);
		// For integral implementation use this instead of the above two lines:
		// if constexpr (std::is_signed_v<T> && std::is_integral_v<T>) {
		//	intersect = (a3 ^ a4) < 0;
		//	collinear = a3 == 0 || a4 == 0;
		//}
		if (intersect) {
			return true;
		}
	}

	bool point_overlap{
		(Overlaps(line.b) || Overlaps(line.a) || line.Overlaps(a) || line.Overlaps(b))
	};

	return collinear && point_overlap;
}

bool Line::Overlaps(const Circle& circle) const {
	return circle.Overlaps(*this);
}

bool Line::Overlaps(const Rect& rect) const {
	return rect.Overlaps(*this);
}

} // namespace ptgn