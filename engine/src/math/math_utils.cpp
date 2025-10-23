#include "math/utility.h"

#include <cmath>
#include <type_traits>
#include <utility>
#include <vector>

#include "common/assert.h"
#include "math/geometry/axis.h"
#include "math/math.h"
#include "math/vector2.h"

namespace ptgn {

namespace impl {

bool WithinPerimeter(float radius, float dist2, bool include_edge) {
	float radius2 = radius * radius;
	if (dist2 < radius2) {
		return true;
	}
	return include_edge && NearlyEqual(dist2, radius2);
}

float ClosestPointLineLine(
	const V2_float& lineA_start, const V2_float& lineA_end, const V2_float& lineB_start,
	const V2_float& lineB_end, float& s, float& t, V2_float& c1, V2_float& c2
) {
	V2_float d1{ lineA_end - lineA_start }; // Direction vector of segment S1
	V2_float d2{ lineB_end - lineB_start }; // Direction vector of segment S2
	V2_float r{ lineA_start - lineB_start };
	float a = d1.Dot(d1);					// Squared length of segment S1, always nonnegative
	float e = d2.Dot(d2);					// Squared length of segment S2, always nonnegative
	float f = d2.Dot(r);
	// Checke if one or both segments degenerate into points.
	if (a <= epsilon<float> && e <= epsilon<float>) {
		// Both segments degenerate into points
		s = t = 0.0f;
		c1	  = lineA_start;
		c2	  = lineB_start;
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
	c1 = lineA_start + d1 * s;
	c2 = lineB_start + d2 * t;
	return (c1 - c2).Dot(c1 - c2);
}

float SquareDistancePointLine(const V2_float& point, const V2_float& start, const V2_float& end) {
	// Source:
	// https://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 130.
	V2_float ab{ end - start };
	V2_float ac{ point - start };
	V2_float bc{ point - end };
	float e = ac.Dot(ab);
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

float SquareDistancePointRect(
	const V2_float& point, const V2_float& rect_min, const V2_float& rect_max
) {
	float dist2{ 0.0f };
	for (std::size_t i{ 0 }; i < 2; ++i) {
		const float v{ point[i] };
		if (v < rect_min[i]) {
			dist2 += (rect_min[i] - v) * (rect_min[i] - v);
		}
		if (v > rect_max[i]) {
			dist2 += (v - rect_max[i]) * (v - rect_max[i]);
		}
	}
	return dist2;
}

float ParallelogramArea(const V2_float& a, const V2_float& b, const V2_float& c) {
	return (a - c).Cross(b - c);
}

std::vector<Axis> GetPolygonAxes(
	const V2_float* vertices, std::size_t vertex_count, [[maybe_unused]] bool intersection_info
) {
	std::vector<Axis> axes;

	const auto parallel_axis_exists = [&axes](const Axis& o_axis) {
		for (const auto& axis : axes) {
			if (NearlyEqual(o_axis.direction.Cross(axis.direction), 0.0f)) {
				return true;
			}
		}
		return false;
	};

	axes.reserve(vertex_count);

	for (std::size_t a{ 0 }; a < vertex_count; a++) {
		std::size_t b{ a + 1 == vertex_count ? 0 : a + 1 };

		Axis axis;
		axis.midpoint  = Midpoint(vertices[a], vertices[b]);
		axis.direction = vertices[a] - vertices[b];

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

std::pair<float, float> GetPolygonProjectionMinMax(
	const V2_float* vertices, std::size_t vertex_count, const Axis& axis
) {
	PTGN_ASSERT(vertex_count > 0);
	PTGN_ASSERT(
		std::invoke([&]() {
			float mag2{ axis.direction.MagnitudeSquared() };
			return mag2 < 1.0f || NearlyEqual(mag2, 1.0f);
		}),
		"Projection axis must be normalized"
	);

	float min{ axis.direction.Dot(vertices[0]) };
	float max{ min };
	for (std::size_t i{ 1 }; i < vertex_count; i++) {
		float p = vertices[i].Dot(axis.direction);
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
		float min_endpoint{ Abs(min_dist) };
		float max_endpoint{ Abs(max_dist) };

		if (max_endpoint > min_endpoint) {
			// Flip projection normal direction.
			out_axis_direction *= -1.0f;
			return internal_dist + min_endpoint;
		}
		return internal_dist + max_endpoint;
	}

	float right_dist{ Abs(min1 - max2) };

	if (max_dist > 0.0f) { // Overlapping the interval from the right.
		return right_dist;
	}

	float left_dist{ Abs(max1 - min2) };

	if (min_dist < 0.0f) { // Overlapping the interval from the left.
		return left_dist;
	}

	// Entirely within the interval.
	return std::min(right_dist, left_dist);
}

bool IsConvexPolygon(const V2_float* vertices, std::size_t vertex_count) {
	PTGN_ASSERT(vertex_count >= 3, "Line or point convexity check is redundant");

	const auto get_cross = [](const V2_float& a, const V2_float& b, const V2_float& c) {
		return (b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x);
	};

	int sign{ static_cast<int>(Sign(get_cross(vertices[0], vertices[1], vertices[2]))) };

	// For convex polygons, all sequential point triplet cross products must have the same sign
	// (+ or -). For convex polygon every triplet makes turn in the same side (or CW, or CCW
	// depending on walk direction). For concave one some signs will differ (where inner angle
	// exceeds 180 degrees). Note that you don't need to calculate angle values. Source:
	// https://stackoverflow.com/a/40739079

	// Skip first point since that is the established reference.
	for (std::size_t i = 1; i < vertex_count; i++) {
		V2_float a{ vertices[(i + 0)] };
		V2_float b{ vertices[(i + 1) % vertex_count] };
		V2_float c{ vertices[(i + 2) % vertex_count] };

		int new_sign{ static_cast<int>(Sign(get_cross(a, b, c))) };

		if (new_sign != sign) {
			// Polygon is concave.
			return false;
		}
	}

	// Convex.
	return true;
}

bool IsConcavePolygon(const V2_float* vertices, std::size_t vertex_count) {
	return !IsConvexPolygon(vertices, vertex_count);
}

} // namespace impl

} // namespace ptgn