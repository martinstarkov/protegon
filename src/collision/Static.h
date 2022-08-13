#pragma once

#include <array>  // std::array
#include <limits> // std::numeric_limits

#include "physics/Types.h"
#include "math/Vector2.h"
#include "math/Math.h"
#include "math/LinearAlgebra.h"
#include "utility/TypeTraits.h"
#include "collision/Overlap.h"

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

// TODO: Replace with SAT algorithm.
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
// Basically do a line vs 4 AABB sides, find min distance, pick that point and do a CirclevsAABB there.

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
		Line<S> line{ a };
		Line<S> other{ b };
		S sign{ -1 };
		// Determine which is the which is the collision normal axis
		// and set the non collision normal axis as the other one.
		if (min_index == 0) {
			max_index = 1;
		} else if (min_index == 1) {
			max_index = 0;
		} else if (min_index == 2) {
			Swap(line.origin, other.origin);
			Swap(line.destination, other.destination);
			sign = 1;
			max_index = 3;
		} else if (min_index == 3) {
			Swap(line.origin, other.origin);
			Swap(line.destination, other.destination);
			sign = 1;
			max_index = 2;
		}
		math::Vector2<S> dir{ line.Direction() };
		math::Vector2<S> o_dir{ other.Direction() };
		// TODO: Perhaps this check could be moved to the very beginning as it does not rely on projections.
		bool zero_dir{ dir.IsZero() };
		bool o_zero_dir{ o_dir.IsZero() };
		if (zero_dir || o_zero_dir) {
			// At least one of the capsules is a circle.
			if (zero_dir && o_zero_dir) {
				// Both capsules are circles.
				// Circle vs circle collision where both circle centers overlap.
				return CircleCircle(Circle{ c1, static_cast<S>(a.radius) }, Circle{ c2, static_cast<S>(b.radius) });
			} else {
				if (zero_dir) {
					// Only one of the capsules is a circle.
					// Capsule vs circle collision where circle center intersects capsule centerline.
					collision.normal = o_dir.Tangent().Normalize();
					collision.penetration = collision.normal * rad_sum;
					return collision;
				} else if (o_zero_dir) {
					// Only one of the capsules is a circle.
					// Capsule vs circle collision where circle center intersects capsule centerline.
					collision.normal = dir.Tangent().Normalize();
					collision.penetration = collision.normal * rad_sum;
					return collision;
				}
			}
		} else {
			// Capsule vs capsule.
			S frac{}; // frac is an unused variable.
			math::Vector2<S> point;
			// TODO: Fix this awful branching.
			// TODO: Clean this up, I'm sure some of these cases can be combined.
			math::ClosestPointInfiniteLine(points[min_index], other, frac, point);
			const math::Vector2<S> vector_to_min{ points[min_index] - point };
			if (vector_to_min.IsZero()) {
				// Capsule centerlines touch in at least one location.
				math::ClosestPointInfiniteLine(points[max_index], other, frac, point);
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

// TODO: Replace with modified SAT algorithm.
// Source: https://steamcdn-a.akamaihd.net/apps/valve/2015/DirkGregorius_Contacts.pdf
// Static rectangle and circle collision detection.
template <typename S = float, typename T,
	tt::floating_point<S> = true>
static Collision<S> CircleAABB(const Circle<T>& a,
							   const AABB<T>& b) {
	Collision<S> collision;

	using Edge = std::pair<math::Vector2<T>, math::Vector2<T>>;
	math::Vector2<T> top_right{ b.position.x + b.size.x, b.position.y };
	math::Vector2<T> bottom_right{ b.position + b.size };
	math::Vector2<T> bottom_left{ b.position.x, b.position.y + b.size.y };
	std::array<Edge, 4> edges;
	edges.at(0) = { b.position, top_right };     // top
	edges.at(1) = { top_right, bottom_right };   // right
	edges.at(2) = { bottom_right, bottom_left }; // bottom
	edges.at(3) = { bottom_left, b.position };   // left
	S min_dist2{ std::numeric_limits<S>::infinity() };
	math::Vector2<S> min_point;
	std::size_t side_index{ 0 };
	for (std::size_t i{ 0 }; i < edges.size(); ++i) {
		auto& [origin, destination] = edges[i];
		S t{};
		math::Vector2<S> c1;
		math::ClosestPointLine<S>(a.center, { origin, destination }, t, c1);
		S dist2{ (a.center - c1).MagnitudeSquared() };
		if (dist2 < min_dist2) {
			side_index = i;
			min_dist2 = dist2;
			// Point on the AABB that was the closest.
			min_point = c1;
		}
	}

	bool inside{ overlap::PointAABB(a.center, b) };

	const T rad2{ a.RadiusSquared() };
	if (min_dist2 > rad2 && !inside)
		return collision;

	collision.SetOccured();

	if (math::Compare(min_dist2, 0)) {
		// Circle is on one of the AABB edges.
		switch (side_index) {
			case 0:
				collision.normal = { 0, -1 }; // top
				break;
			case 1:
				collision.normal = { 1, 0 };  // right
				break;
			case 2:
				collision.normal = { 0, 1 };  // bottom
				break;
			case 3:
				collision.normal = { -1, 0 }; // left
				break;
		}
		collision.penetration = collision.normal * a.radius;
		return collision;
	} else {
		const math::Vector2<S> dir{ a.center - min_point };
		const S mag{ dir.Magnitude() };

		// TODO: Move this to the very beginning as it is an edge case 
		// which can be checked before looping all sides of aabb.
		if (math::Compare(mag, 0))
			// Choose upward vector arbitrarily if circle center is aabb center.
			collision.normal = { 0, -1 }; // top
		else
			collision.normal = dir / mag;

		if (inside) {
			collision.normal *= -1;
			collision.penetration = collision.normal * (a.radius + mag);
		} else {
			collision.penetration = collision.normal * (a.radius - mag);
		}
		return collision;
	}
}

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
	if (dist2 > rad_sum2 || math::Compare(dist2, rad_sum2)) {
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

// TODO: Implement LineAABB.

// Get the collision information of a line and a capsule.
// Capsule origin and destination are taken from the edge circle centers.
template <typename S = float, typename T,
	tt::floating_point<S> = true>
static Collision<S> LineCapsule(const Line<T>& a,
								const Capsule<T>& b) {
	// TODO: Make this more efficient.
	return CapsuleCapsule(Capsule{ a.origin, a.destination, 0 }, b);
}

// TODO: Implement LineCircle collisions.
// Intersects ray r = p + td, |d| = 1, with sphere s and, if intersecting,
// returns t value of intersection and intersection point q
template <typename S = float, typename T,
	tt::floating_point<S> = true>
static Collision<S> LineCircle(const Line<T>& a,
							   const Circle<T>& b) {
	return CapsuleCapsule(Capsule{ a.origin, a.destination, 0 }, Capsule{ b.center, b.center, b.radius });
}

// Get the collision information of two lines.
template <typename S = float, typename T,
	tt::floating_point<S> = true>
static Collision<S> LineLine(const Line<T>& a,
							 const Line<T>& b) {
	// TODO: Make this more efficient.
	return CapsuleCapsule(Capsule{ a.origin, a.destination, 0 }, Capsule{ b.origin, b.destination, 0 });
}

// Static collision check between a point and an aabb with collision information.
template <typename T, typename S = float,
	tt::floating_point<S> = true>
inline Collision<S> PointAABB(const Point<T>& a,
							  const AABB<T>& b) {
	return AABBAABB(AABB{ a, {} }, AABB{ b.position, b.size });
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