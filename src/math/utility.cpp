#include "math/utility.h"

#include <cmath>
#include <type_traits>
#include <utility>
#include <vector>

#include "geometry/axis.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/math.h"
#include "math/vector2.h"
#include "utility/assert.h"

namespace ptgn {

namespace impl {

bool WithinPerimeter(float radius, float dist2) {
	float rad2{ radius * radius };

	if (dist2 < rad2 && !NearlyEqual(dist2, rad2)) {
		return true;
	}

	// Optional: Include perimeter:
	/*if (NearlyEqual(dist2, rad2)) {
		return true;
	}*/

	return false;
}

float ClosestPointLineLine(
	const Line& line1, const Line& line2, float& s, float& t, V2_float& c1, V2_float& c2
) {
	V2_float d1 = line1.Direction(); // Direction vector of segment S1
	V2_float d2 = line2.Direction(); // Direction vector of segment S2
	V2_float r	= line1.a - line2.a;
	float a		= d1.Dot(d1);		 // Squared length of segment S1, always nonnegative
	float e		= d2.Dot(d2);		 // Squared length of segment S2, always nonnegative
	float f		= d2.Dot(r);
	// Checke if one or both segments degenerate into points.
	if (a <= epsilon<float> && e <= epsilon<float>) {
		// Both segments degenerate into points
		s = t = 0.0f;
		c1	  = line1.a;
		c2	  = line2.a;
		return (c1 - c2).Dot(c1 - c2);
	}
	if (a <= epsilon<float>) {
		// First segment degenerates into a point
		s = 0.0f;
		t = f / e; // s = 0 => t = (b*s + f) / e = f / e
		t = std::clamp(t, 0.0f, 1.0f);
	} else {
		float c = d1.Dot(r);
		if (e <= epsilon<float>) {
			// Second segment degenerates into a point
			t = 0.0f;
			s = std::clamp(-c / a, 0.0f, 1.0f); // t = 0 => s = (b*t - c) / a = -c / a
		} else {
			// The general nondegenerate case starts here
			float b		= d1.Dot(d2);
			float denom = a * e - b * b; // Always nonnegative
			// If segments not parallel, compute closest point on L1 to L2 and
			// clamp to segment S1. Else pick arbitrary s (here 0)
			if (denom != 0.0f) {
				s = std::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
			} else {
				s = 0.0f;
			}

			// Compute point on L2 closest to S1(s) using
			// t = Dot((P1 + D1*s) - P2,D2) / Dot(D2,D2) = (b*s + f) / e
			float tnom = b * s + f;

			if (tnom < 0.0f) {
				t = 0.0f;
				s = std::clamp(-c / a, 0.0f, 1.0f);
			} else if (tnom > e) {
				t = 1.0f;
				s = std::clamp((b - c) / a, 0.0f, 1.0f);
			} else {
				t = tnom / e;
			}
		}
	}
	c1 = line1.a + d1 * s;
	c2 = line2.a + d2 * t;
	return (c1 - c2).Dot(c1 - c2);
}

float SquareDistancePointLine(const Line& line, const V2_float& c) {
	// Source:
	// https://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 130.
	V2_float ab = line.b - line.a;
	V2_float ac = c - line.a;
	V2_float bc = c - line.b;
	float e		= ac.Dot(ab);
	// Handle cases where c projects outside ab
	if (e <= 0.0f) {
		return ac.Dot(ac);
	}
	float f = ab.Dot(ab);
	if (e >= f) {
		return bc.Dot(bc);
	}
	// Handle cases where c projects onto ab
	return ac.Dot(ac) - e * e / f;
}

float SquareDistancePointRect(const V2_float& a, const Rect& b) {
	float dist2{ 0.0f };
	V2_float max{ b.Max() };
	V2_float min{ b.Min() };
	for (std::size_t i{ 0 }; i < 2; ++i) {
		const float v{ a[i] };
		if (v < min[i]) {
			dist2 += (min[i] - v) * (min[i] - v);
		}
		if (v > max[i]) {
			dist2 += (v - max[i]) * (v - max[i]);
		}
	}
	return dist2;
}

float ParallelogramArea(const V2_float& a, const V2_float& b, const V2_float& c) {
	return (a - c).Cross(b - c);
}

std::vector<Axis> GetAxes(const Polygon& polygon, [[maybe_unused]] bool intersection_info) {
	std::vector<Axis> axes;

	const auto parallel_axis_exists = [&axes](const Axis& o_axis) {
		for (const auto& axis : axes) {
			if (NearlyEqual(o_axis.direction.Cross(axis.direction), 0.0f)) {
				return true;
			}
		}
		return false;
	};

	axes.reserve(polygon.vertices.size());

	for (std::size_t a{ 0 }; a < polygon.vertices.size(); a++) {
		std::size_t b{ a + 1 == polygon.vertices.size() ? 0 : a + 1 };

		Axis axis;
		axis.midpoint  = Midpoint(polygon.vertices[a], polygon.vertices[b]);
		axis.direction = polygon.vertices[a] - polygon.vertices[b];

		// Skip coinciding points with no axis.
		if (axis.direction.IsZero()) {
			continue;
		}

		axis.direction = axis.direction.Skewed();

		// TODO: Check if this still produces accurate results.
		// if (intersection_info) {
		axis.direction = axis.direction.Normalized();
		//}

		if (!parallel_axis_exists(axis)) {
			axes.emplace_back(axis);
		}
	}

	return axes;
}

std::pair<float, float> GetProjectionMinMax(const Polygon& polygon, const Axis& axis) {
	PTGN_ASSERT(!polygon.vertices.empty());
	PTGN_ASSERT(
		std::invoke([&]() {
			float mag2{ axis.direction.MagnitudeSquared() };
			return mag2 < 1.0f || NearlyEqual(mag2, 1.0f);
		}),
		"Projection axis must be normalized"
	);

	float min{ axis.direction.Dot(polygon.vertices.front()) };
	float max{ min };
	for (std::size_t i{ 1 }; i < polygon.vertices.size(); i++) {
		float p = polygon.vertices[i].Dot(axis.direction);
		if (p < min) {
			min = p;
		} else if (p > max) {
			max = p;
		}
	}

	return { min, max };
}

bool IntervalsOverlap(float min1, float max1, float min2, float max2) {
	return !(min1 > max2 || min2 > max1);
}

float GetIntervalOverlap(
	float min1, float max1, float min2, float max2, bool contained_polygon,
	V2_float& out_axis_direction
) {
	// TODO: Combine the contained_polygon case into the regular overlap logic if possible.

	if (!IntervalsOverlap(min1, max1, min2, max2)) {
		return 0.0f;
	}

	float min_dist{ min1 - min2 };
	float max_dist{ max1 - max2 };

	if (contained_polygon) {
		float internal_dist{ std::min(max1, max2) - std::max(min1, min2) };

		// Get the overlap plus the distance from the minimum end points.
		float min_endpoint{ FastAbs(min_dist) };
		float max_endpoint{ FastAbs(max_dist) };

		if (max_endpoint > min_endpoint) {
			// Flip projection normal direction.
			out_axis_direction *= -1.0f;
			return internal_dist + min_endpoint;
		}
		return internal_dist + max_endpoint;
	}

	float right_dist{ FastAbs(min1 - max2) };

	if (max_dist > 0.0f) { // Overlapping the interval from the right.
		return right_dist;
	}

	float left_dist{ FastAbs(max1 - min2) };

	if (min_dist < 0.0f) { // Overlapping the interval from the left.
		return left_dist;
	}

	// Entirely within the interval.
	return std::min(right_dist, left_dist);
}

} // namespace impl

} // namespace ptgn
