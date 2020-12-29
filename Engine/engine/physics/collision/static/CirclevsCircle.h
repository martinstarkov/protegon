#pragma once

#include "renderer/Circle.h"

#include "physics/collision/CollisionManifold.h"

namespace engine {

namespace collision {

// Static collision check between two circles.
bool CirclevsCircle(const Circle& A, const Circle& B) {
	double r = A.radius + B.radius;
	r *= r;
	return r < (A.position + B.position).MagnitudeSquared();
}

// Static collision check between two circles with collision information.
bool IntersectionCirclevsCircle(const Circle& A, const Circle& B, CollisionManifold& out_collision) {
	// Vector from A to B
	V2_double n = B.position - A.position;

	double r = A.radius + B.radius;
	r *= r;

	if (n.MagnitudeSquared() > r) {
		return false;
	}
	// Circles have collided, now compute manifold.
	double d = n.Magnitude(); // perform actual sqrt

	// If distance between circles is not zero
	if (d) {
		// Distance is difference between radius and distance
		out_collision.depth = r - d;
		// Utilize our d since we performed sqrt on it already within magnitude()
		// Points from A to B, and is a unit vector
		out_collision.normal = n / d;
	} else { // Circles are in the same position
		// Choose random (but consistent) values.
		out_collision.depth = A.radius;
		out_collision.normal = V2_double{ 1.0, 0.0 };
	}

	return true;
}

} // namespace collision

} // namespace engine