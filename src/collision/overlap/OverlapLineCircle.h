#pragma once

#include "math/Vector2.h"

// Source: https://www.jeffreythompson.org/collision-detection/line-circle.php
// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 179.
// Source: https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection

namespace ptgn {

namespace math {

static bool SolveQuadratic(const double& a, const double& b, const double& c, double& x0, double& x1) {
	double discr = b * b - 4 * a * c;
	if (discr < 0) return false;
	else if (math::Compare(discr, 0.0)) {
		x0 = x1 = -0.5 * b / a;
	} else {
		double q = (b > 0) ?
			-0.5 * (b + math::Sqrt(discr)) :
			-0.5 * (b - math::Sqrt(discr));
		x0 = q / a;
		x1 = c / q;
	}

	return true;
}

} // namespace math

namespace collision {

namespace overlap {

// TODO: Fix this...

// Check if a line and a circle overlap.
// Circle position is taken from its center.
template <typename T>
static bool LinevsCircle(const math::Vector2<T>& line_origin,
					     const math::Vector2<T>& line_destination,
					     const math::Vector2<T>& circle_position,
					     const T circle_radius) {


	/*
	// is either end INSIDE the circle?
    // if so, return true immediately
	const bool inside1 = PointvsCircle(line_origin, circle_position, circle_radius);
	const bool inside2 = PointvsCircle(line_destination, circle_position, circle_radius);
	if (inside1 || inside2) return true;

	// get length of the line
	V2_double dist = line_origin - line_destination;
	const double len_squared = dist.Magnitude();

	// get dot product of the line and circle
	const double dot = (((circle_position.x - line_origin.x) * (line_destination.x - line_origin.x)) + ((circle_position.y - line_origin.y) * (line_destination.y - line_origin.y))) / len_squared;

	// find the closest point on the line
	const V2_double closest = line_origin + (dot * (line_destination - line_origin));

	// is this point actually on the line segment?
	// if so keep going, but if not, return false
	const bool onSegment = PointvsLine<double>(closest, line_origin, line_destination);
	if (!onSegment) return false;

	// get distance to closest point
	dist = closest - circle_position;
	const double distance_squared = dist.MagnitudeSquared();
	const double radius_squared{ static_cast<double>(circle_radius) * static_cast<double>(circle_radius) };

	if (distance_squared < radius_squared || math::Compare(distance_squared, radius_squared)) {
		return true;
	}
	return false;
	*/
}

} // namespace overlap

} // namespace collision

} // namespace ptgn