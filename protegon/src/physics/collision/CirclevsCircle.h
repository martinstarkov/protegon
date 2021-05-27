#pragma once

#include "math/Vector2.h"
#include "physics/Manifold.h"
#include "physics/shapes/Circle.h"

namespace engine {

namespace math {

// Static collision check between two circles.
inline bool CirclevsCircle(const Circle& shapeA, 
						   const V2_double& positionA, 
						   const Circle& shapeB, 
						   const V2_double& positionB) {
	double radius_combined{ shapeA.radius + shapeB.radius };
	radius_combined *= radius_combined;
	return radius_combined < (positionA + positionB).MagnitudeSquared();
}

// Static collision check between two circles with collision information.
inline Manifold IntersectionCirclevsCircle(const Circle& shapeA,
										   const V2_double& positionA,
										   const Circle& shapeB,
										   const V2_double& positionB) {
	Manifold manifold;
	// Vector from A to B
	const V2_double n{ positionB - positionA };

	double radius_combined{ shapeA.radius + shapeB.radius };
	radius_combined *= radius_combined;

	const auto n_squared{ n.MagnitudeSquared() };

	if (radius_combined < n_squared) {
		return manifold;
	}

	// Circles have collided, now compute manifold.
	const double d{ math::Sqrt(n_squared) }; // perform actual sqrt

	// If distance between circles is not zero
	if (d != 0) {
		// Distance is difference between radius and distance
		const auto penetration{ radius_combined - d };
		// Utilize our d since we performed sqrt on it already within magnitude()
		// Points from A to B, and is a unit vector
		manifold.normal = n / d;
		manifold.penetration = manifold.normal * penetration;
		manifold.contact_point = positionA + manifold.penetration / 2.0;
	} else { // Circles are in the same position
		// Choose random (but consistent) values.
		manifold.penetration = { shapeA.radius, shapeA.radius };
		manifold.normal = { 1.0, 0.0 };
		manifold.contact_point = positionA;
	}

	return manifold;
}

} // namespace math

} // namespace engine