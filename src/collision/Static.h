#pragma once

#include <array>  // std::array
#include <limits> // std::numeric_limits

#include "physics/Types.h"
#include "math/Vector2.h"
#include "math/Math.h"
#include "math/LinearAlgebra.h"
#include "utility/TypeTraits.h"

namespace ptgn {

namespace intersect {

template <typename T,
    tt::floating_point<T> = true>
struct Collision {
    Collision() = default;
    ~Collision() = default;
    bool Occured() const {
        return occured_;
    }
    // Normal vector to the collision plane.
    math::Vector2<T> normal;
    // Penetration of objects into each other along the collision normal.
    math::Vector2<T> penetration;
    void SetOccured() {
        occured_ = true;
    }
    private:
        bool occured_{ false };
};

// Static collision check between two aabbs with collision information.
template <typename T, typename S = float,
    tt::floating_point<S> = true>
static Collision<S> AABBAABB(const AABB<T>& a,
                             const AABB<T>& b) {
    Collision<S> collision;
    const S a_half_x{ a.size.x / 2.0f };
    const S b_half_x{ b.size.x / 2.0f };
    const S direction_x{ b.position.x + b_half_x - (a.position.x + a_half_x) };
    const S penetration_x{ a_half_x + b_half_x - math::FastAbs(direction_x) };
    if (penetration_x < 0 || math::Compare(penetration_x, 0)) {
        return collision;
    }
    const S a_half_y{ a.size.y / 2.0f };
    const S b_half_y{ b.size.y / 2.0f };
    const S direction_y{ b.position.y + b_half_y - (a.position.y + a_half_y) };
    const S penetration_y{ a_half_y + b_half_y - math::FastAbs(direction_y) };
    if (penetration_y < 0 || math::Compare(penetration_y, 0)) {
        return collision;
    }

    collision.SetOccured();

    if (math::Compare(direction_x, 0) && math::Compare(direction_y, 0)) {
        // Edge case where aabb centers are in the same location, 
        // choose arbitrary upward normal to resolve this.
        collision.normal.y = -1;
        collision.penetration = collision.normal * (a_half_y + b_half_y);
    } else if (penetration_x < penetration_y) {
        const S sign_x{ math::Sign(direction_x) };
        collision.normal.x = -sign_x;
        collision.penetration = collision.normal * math::FastAbs(penetration_x);
    } else {
        const S sign_y{ math::Sign(direction_y) };
        collision.normal.y = -sign_y;
        collision.penetration = collision.normal * math::FastAbs(penetration_y);
    }
    //collision.point = a.position + collision.normal * collision.penetration;
    return collision;
}

// Source: https://steamcdn-a.akamaihd.net/apps/valve/2015/DirkGregorius_Contacts.pdf
// TODO: Implement CapsuleAABB here.


// Source: https://steamcdn-a.akamaihd.net/apps/valve/2015/DirkGregorius_Contacts.pdf
// With edge cases added.
// Get the collision information of two overlapping capsules.
// Capsule origins and destinations are taken from the edge circle centers.
template <typename S = float, typename T,
	tt::floating_point<S> = true>
static Collision<S> CapsuleCapsule(const Capsule<T>& a,
								   const Capsule<T>& b) {
	// TODO: Make this function much more streamlined.
	static_assert(!tt::is_narrowing_v<T, S>);
	Collision<S> collision;
	// Compute (squared) distance between the inner structures of the capsules.
	S s{};
	S t{};
	math::Vector2<S> c1;
	math::Vector2<S> c2;
	const S dist2{ math::ClosestPointLineLine<S>(a, b, s, t, c1, c2) };
	// If (squared) distance smaller than (squared) sum of radii, they collide
	const S rad_sum{ static_cast<S>(a.radius) + static_cast<S>(b.radius) };
	const S rad_sum2{ rad_sum * rad_sum };
	if (!(dist2 < rad_sum2 ||
		  math::Compare(dist2, rad_sum2))) {
		return collision;
	}
	collision.SetOccured();
	if (math::Compare(dist2, 0)) {
		// Capsules lines intersect, different kind of routine needed.
		std::array<math::Vector2<S>, 4> points;
		points[0] = a.origin;
		points[1] = a.destination;
		points[2] = b.origin;
		points[3] = b.destination;

		// Find shortest distance (and index) to 4 capsule end points (2 per capsule).
		S min_dist2{ std::numeric_limits<S>::infinity() };
		std::size_t min_index{ 0 };
		std::size_t max_index{ 0 };
		for (std::size_t i{ 0 }; i < points.size(); ++i) {
			const S d{ DistanceSquared(points[i], c1) };
			if (d < min_dist2) {
				min_index = i;
				min_dist2 = d;
			}
		}
		math::Vector2<S> origin{ a.origin };
		math::Vector2<S> destination{ a.destination };
		math::Vector2<S> other_origin{ b.origin };
		math::Vector2<S> other_destination{ b.destination };
		S sign{ -1 };
		// Determine which is the which is the collision normal axis
		// and set the non collision normal axis as the other one.
		if (min_index == 0) {
			max_index = 1;
		} else if (min_index == 1) {
			max_index = 0;
		} else if (min_index == 2) {
			std::swap(origin, other_origin);
			std::swap(destination, other_destination);
			sign = 1;
			max_index = 3;
		} else if (min_index == 3) {
			std::swap(origin, other_origin);
			std::swap(destination, other_destination);
			sign = 1;
			max_index = 2;
		}
		math::Vector2<S> dir{ destination - origin };
		math::Vector2<S> o_dir{ other_destination - other_origin };
		// TODO: Perhaps this check could be moved to the very beginning as it does not rely on projections.
		if (dir.IsZero()) {
			// At least one of the capsules is a circle.
			if (o_dir.IsZero()) {
				// Both capsules are circles.
				// Circle vs circle collision where both circle centers overlap.
				return CircleCircle(Circle{ c1, static_cast<S>(a.radius) }, Circle{ c2, static_cast<S>(b.radius) });
			} else {
				// Only one of the capsules is a circle.
				// Capsule vs circle collision where circle center intersects capsule centerline.
				collision.normal = o_dir.Tangent().Normalize();
				collision.penetration = collision.normal * rad_sum;
				return collision;
			}
		} else {
			// Capsule vs capsule.
			S frac{}; // frac is an unused variable.
			math::Vector2<S> point;
			// TODO: Fix this awful branching.
			// TODO: Clean this up, I'm sure some of these cases can be combined.
			math::ClosestPointInfiniteLine(points[min_index], other_origin, other_destination, frac, point);
			const math::Vector2<S> vector_to_min{ points[min_index] - point };
			if (vector_to_min.IsZero()) {
				// Capsule centerlines touch in at least one location.
				math::ClosestPointInfiniteLine(points[max_index], other_origin, other_destination, frac, point);
				const math::Vector2<S> vector_to_max{ -(points[max_index] - point).Normalize() };
				if (vector_to_max.IsZero()) {
					// Capsules are collinear.
					const S penetration{ Distance(points[min_index], point) + rad_sum };
					if (penetration > rad_sum) {
						// Push capsules apart in perpendicular direction.
						collision.normal = -dir.Tangent().Normalize();
						collision.penetration = collision.normal * rad_sum;
						return collision;
					} else {
						// Push capsules apart in parallel direction.
						collision.normal = sign * -dir.Normalize();
						collision.penetration = collision.normal * penetration;
						return collision;
					}
				} else {
					// Capsule origin or destination lies on the other capsule's centerline.
					collision.normal = sign * vector_to_max;
					collision.penetration = collision.normal * rad_sum;
					return collision;
				}
			} else {
				// Capsule centerlines intersect each other.
				collision.normal = sign * vector_to_min.Normalize();
				collision.penetration = collision.normal * (Distance(points[min_index], point) + rad_sum);
				return collision;
			}
		}
	} else {
		// Capsule centerlines do not intersect each other.
		return CircleCircle(Circle{ c1, static_cast<S>(a.radius) }, Circle{ c2, static_cast<S>(b.radius) });
	}
	return collision;
}

// TODO: Fix CircleAABB function.
/*
// Source: https://steamcdn-a.akamaihd.net/apps/valve/2015/DirkGregorius_Contacts.pdf
// Static rectangle and circle collision detection.
inline Collision<S> CircleAABB(const Circle& shapeA,
							   const V2_double& positionA,
							   const AABB& shapeB,
							   const V2_double& positionB) {
	Collision<S> collision;
	// Vector from A to B.
	const V2_double half{ shapeB.size / 2.0 };
	const V2_double n{ positionA - (positionB + half) };
	// Closest point on A to center of B.
	V2_double closest{ n };
	// Clamp point to edges of the AABB.
	closest = std::clamp(closest, -half, half);
	bool inside{ false };
	// Circle is inside the AABB, so we need to clamp the circle's center
	// to the closest edge
	if (n == closest) {
		inside = true;
		// Find closest axis
		if (math::FastAbs(n.x) > math::FastAbs(n.y)) {
			// Clamp to closest extent
			if (closest.x > 0.0) {
				closest.x = half.x;
			} else {
				closest.x = -half.x;
			}
		} else { // y axis is shorter
			// Clamp to closest extent
			if (closest.y > 0.0) {
				closest.y = half.y;
			} else {
				closest.y = -half.y;
			}
		}
	}

	const auto normal{ n - closest };
	const auto dist2{ normal.MagnitudeSquared() };
	// Early out of the radius is shorter than distance to closest point and
	// Circle not inside the AABB
	if (dist2 > shapeA.radius * shapeA.radius && !inside) {
		return collision;
	}
	// Avoid sqrtf until we needed to take it.
	auto distance{ std::sqrtf(dist2) };
	// Collision normal needs to be flipped to point outside if circle was
	// inside the AABB
	if (inside) {
		collision.normal = -n;
	} else {
		collision.normal = n;
	}
	collision.penetration = collision.normal * (shapeA.radius - distance);
	collision.contact_point = positionA + collision.penetration;
	return collision;
}
*/

// Source: https://steamcdn-a.akamaihd.net/apps/valve/2015/DirkGregorius_Contacts.pdf
// Get the collision information of a circle and a capsule.
// Capsule origin and destination are taken from the edge circle centers.
template <typename S = float, typename T,
	tt::floating_point<S> = true>
static Collision<S> CircleCapsule(const Circle<T>& a,
								  const Capsule<T>& b) {
	return CapsuleCapsule<S>({ a.center, a.center, a.radius }, b);
}

// Static collision check between two circles with collision information.
template <typename T, typename S = float,
	tt::floating_point<S> = true>
static Collision<S> CircleCircle(const Circle<T>& a,
								 const Circle<T>& b) {
	static_assert(!tt::is_narrowing_v<T, S>);
	Collision<S> collision;

	const math::Vector2<T> dir{ b.center - a.center };
	const T dist2{ dir.MagnitudeSquared() };
	const T rad_sum{ a.radius + b.radius };
	const T rad_sum2{ rad_sum * rad_sum };

	// Collision did not occur, exit with empty collision.
	if (dist2 > rad_sum2 ||
		math::Compare(dist2, rad_sum2)) {
		return collision;
	}

	collision.SetOccured();

	const S dist{ std::sqrtf(dist2) };

	// Bias toward selecting first circle for exact overlap edge case.
	if (math::Compare(dist, 0)) {
		// Arbitrary normal chosen upward.
		collision.normal = { 0, -1 };
		collision.penetration = collision.normal * rad_sum;
	} else {
		// Normalise collision vector.
		collision.normal = dir / dist;
		// Find the amount by which circles overlap.
		collision.penetration = collision.normal * (dist - static_cast<S>(rad_sum));
	}
	// Find point of collision from the first circle.
	//collision.point = circle_position + collision.penetration * collision.normal;
	return collision;
}

// TODO: Fix LineAABB.
/*
// Return collision manifold between line and an AABB.
inline std::pair<double, Collision<S>> LineAABB(const V2_double& line_origin,
												const V2_double& line_direction,
												const AABB& shape,
												const V2_double& position) {

	Collision<S> collision;

	// Cache division.
	const V2_double inverse_direction{ 1.0 / line_direction };

	// Calculate intersections with rectangle bounding axes.
	V2_double t_near{ (position - line_origin) * inverse_direction };
	V2_double t_far{ (position + shape.size - line_origin) * inverse_direction };

	// Discard 0 / 0 divisions.
	if (std::isnan(t_far.y) || std::isnan(t_far.x)) {
		return { 1.0, collision };
	}
	if (std::isnan(t_near.y) || std::isnan(t_near.x)) {
		return { 1.0, collision };
	}

	// Sort axis collision times so t_near contains the shorter time.
	if (t_near.x > t_far.x) {
		std::swap(t_near.x, t_far.x);
	}
	if (t_near.y > t_far.y) {
		std::swap(t_near.y, t_far.y);
	}

	// Early rejection.
	if (t_near.x > t_far.y || t_near.y > t_far.x) return { 1.0, collision };

	// Closest time will be the first contact.
	auto t_hit_near{ std::max(t_near.x, t_near.y) };

	// Furthest time is contact on opposite side of target.
	auto t_hit_far{ std::min(t_far.x, t_far.y) };

	// Reject if furthest time is negative, meaning the object is travelling away from the target.
	if (t_hit_far < 0.0) {
		return { 1.0, collision };
	}

	// Contact point of collision from parametric line equation.
	//collision.point = line_origin + line_direction * t_hit_near;

	// Find which axis collides further along the movement time.
	if (t_near.x > t_near.y) { // X-axis.
		// Direction of movement.
		if (inverse_direction.x < 0.0) {
			collision.normal = { 1.0, 0.0 };
		} else {
			collision.normal = { -1.0, 0.0 };
		}
	} else if (t_near.x < t_near.y) { // Y-axis.
		// Direction of movement.
		if (inverse_direction.y < 0.0) {
			collision.normal = { 0.0, 1.0 };
		} else {
			collision.normal = { 0.0, -1.0 };
		}
	} else if (t_near.x == t_near.y && t_far.x == t_far.y) { // Both axes collide at the same time.
		// Diagonal collision, set normal to opposite of direction of movement.
		collision.normal = line_direction.Identity().Opposite();
	}

	collision.penetration = line_direction * (t_hit_far - t_hit_near) * collision.normal;

	// Raycast collision occurred.
	return { t_hit_near, collision };
}
*/

// Get the collision information of a line and a capsule.
// Capsule origin and destination are taken from the edge circle centers.
template <typename S = float, typename T,
	tt::floating_point<S> = true>
static Collision<S> LineCapsule(const Line<T>& a,
								const Capsule<T>& b) {
	// TODO: Make this more efficient.
	return CapsuleCapsule(Capsule{ a.origin, a.destination, T{ 0 } }, b);
}

// TODO: Implement LineCircle collisions.

// Get the collision information of two lines.
template <typename S = float, typename T,
	tt::floating_point<S> = true>
static Collision<S> LineLine(const Line<T>& a,
							 const Line<T>& b) {
	// TODO: Make this more efficient.
	return CapsuleCapsule(Capsule{ a.origin, a.destination, T{ 0 } }, Capsule{ b.origin, b.destination, T{ 0 } });
}

// Static collision check between a point and an aabb with collision information.
template <typename T, typename S = float,
	tt::floating_point<S> = true>
inline Collision<S> PointAABB(const Point<T>& a,
							  const AABB<T>& b) {
	return AABBAABB(AABB{ a, { T{ 0 }, T{ 0 } } }, AABB{ b.position, b.size });
}

// Get the collision information of a point and a capsule.
// Capsule origin and destination are taken from the edge circle centers.
template <typename S = float, typename T,
	tt::floating_point<S> = true>
static Collision<S> PointCapsule(const Point<T>& a,
								 const Capsule<T>& b) {
	static_assert(!tt::is_narrowing_v<T, S>);
	Collision<S> collision;
	S t{};
	math::Vector2<S> d;
	// Compute (squared) distance between sphere center and capsule line segment.
	math::ClosestPointLine<S>(a, b, t, d);
	const math::Vector2<S> vector{ d - a };
	const S dist2{ vector.MagnitudeSquared() };
	// If (squared) distance smaller than (squared) sum of radii, they collide.
	const S rad2{ static_cast<S>(b.RadiusSquared()) };
	if (!(dist2 < rad2 || math::Compare(dist2, rad2))) {
		return collision;
	}
	collision.SetOccured();
	if (math::Compare(dist2, 0)) {
		// Point is on the capsule's centerline.
		math::Vector2<S> dir{ b.Direction() };
		if (dir.IsZero()) {
			// Point vs circle where point is at circle center.
			collision.normal = { 0, -1 };
			collision.penetration = collision.normal * b.radius;
		} else {
			// Point vs capsule where point is on capsule centerline.
			T min_dist{ std::min(DistanceSquared(a, b.origin), DistanceSquared(a, b.destination)) };
			if (min_dist > 0) {
				// Push point apart in perpendicular direction (closer).
				collision.normal = -dir.Tangent().Normalize();
				collision.penetration = collision.normal * b.radius;
			} else {
				// Push point apart in parallel direction (closer).
				collision.normal = -dir.Normalize();
				collision.penetration = collision.normal * (std::sqrtf(min_dist) + b.radius);
			}
		}
	} else {
		// Point is within capsule but not on centerline.
		const S dist{ std::sqrtf(dist2) };
		// TODO: Check for division by zero?
		collision.normal = vector / dist;
		// Find the amount by which circles overlap.
		collision.penetration = collision.normal * (dist - static_cast<S>(b.radius));
	}
	return collision;
}

// Static collision check between a circle and a point with collision information.
template <typename T, typename S = float,
	tt::floating_point<S> = true>
inline Collision<S> PointCircle(const Point<T>& a,
								const Circle<T>& b) {
	return CircleCircle({ a, 0 }, b);
}

} // namespace intersect

} // namespace ptgn