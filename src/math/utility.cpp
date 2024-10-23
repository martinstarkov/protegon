#pragma once

#include <cmath>
#include <type_traits>
#include <utility>
#include <vector>

#include "geometry/axis.h"
#include "math/geometry/polygon.h"
#include "math/math.h"
#include "math/vector2.h"
#include "utility/debug.h"

namespace ptgn {

namespace impl {

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

std::vector<Axis> GetAxes(const Polygon& polygon, bool intersection_info) {
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
	PTGN_ASSERT(polygon.vertices.size() > 0);
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