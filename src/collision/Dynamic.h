#pragma once

namespace ptgn {

namespace dynamic {

// TODO: Change this to match dynamic collisions.
template <typename T,
	tt::floating_point<T> = true>
struct Collision {
	Collision() = default;
	~Collision() = default;
	bool Occured() const {
		return occured_;
	}
	// Normal vector to the collision plane.
	Vector2<T> normal;
	// Penetration of objects into each other along the collision normal.
	Vector2<T> penetration;
	void SetOccured() {
		occured_ = true;
	}
	private:
		bool occured_{ false };
};

/*
// TODO: Fix AABBAABB dynamic.
// Determine the time at which a dynamic AABB would collide with a static AABB.
// Pair contains collision time.
inline std::pair<float, Collision<S>> AABBAABB(const AABB& dynamic_shape,
													 const V2_double& dynamic_position,
													 const V2_double& dynamic_velocity,
													 const AABB& static_shape,
													 const V2_double& static_position) {
	Collision<S> collision;

	// Check if dynamic object has a non-zero velocity. It cannot collide if it is not moving.
	if (dynamic_velocity.IsZero()) {
		return { 1.0, collision };
	}

	V2_double dynamic_half{ dynamic_shape.size / 2.0 };
	// Expand static target by dynamic object dimensions so that 
	// only the center of the dynamic object needstobe considered.
	V2_double relative_position{ static_position - dynamic_half };
	AABB combined_shape{ static_shape.size + dynamic_shape.size };

	V2_double dynamic_center{ dynamic_position + dynamic_half };
	// Check if the velocity ray collides with the expanded target.
	auto [nearest_time, m] = IntersectionLinevsAABB(dynamic_center,
													dynamic_velocity,
													combined_shape,
													relative_position);

	collision = m;

	if (collision.Occured() && nearest_time >= 0.0 && nearest_time < 1.0) {
		return { nearest_time, collision };
	} else {
		collision.normal = {};
		collision.penetration = {};
		//collision.point = {};
		return { 1.0, collision };
	}
}

// Modify the velocity of a dynamic AABB so it does not collide with a static AABB.
inline std::pair<float, Collision<S>> ResolveAABBAABB(const AABB& dynamic_shape,
													   const V2_double& dynamic_position,
													   V2_double& dynamic_velocity,
													   const AABB& static_shape,
													   const V2_double& static_position) {
	auto [nearest_time, collision] = AABBAABB(dynamic_shape,
													  dynamic_position,
													  dynamic_velocity,
													  static_shape,
													  static_position);
	// Repeat check is needed due to the fact that if multiple collisions are found, resolving the velocityforthe nearest one may invalidate the previously thought collisions.
	if (collision.Occured()) {
		dynamic_velocity += collision.normal * math::FastAbs(dynamic_velocity) * (1.0 - nearest_time);
		return { nearest_time, collision };
	}
	return { 1.0, collision };
}
*/

// TODO: Implement CapsuleAABB dynamic.

// TODO: Implement CapsuleCapsule dynamic.

// TODO: Implement CircleAABB dynamic.

// TODO: Implement CircleCapsule dynamic.

// TODO: Implement CircleCircle dynamic.

// TODO: Implement LineAABB dynamic.
/*
// Return collision manifold between line and an AABB.
inline std::pair<float, Collision<S>> LineAABB(const V2_float& line_origin,
												const V2_float& line_direction,
												const AABB& shape,
												const V2_float& position) {

	Collision<S> collision;

	// Cache division.
	const V2_float inverse_direction{ 1.0 / line_direction };

	// Calculate intersections with rectangle bounding axes.
	V2_float t_near{ (position - line_origin) * inverse_direction };
	V2_float t_far{ (position + shape.size - line_origin) * inverse_direction };

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

// TODO: Implement LineCapsule dynamic.

// TODO: Implement LineCircle dynamic.

// TODO: Implement LineLine dynamic.

// TODO: Implement PointAABB dynamic.

// TODO: Implement PointCapsule dynamic.

// TODO: Implement PointCircle dynamic.

} // namespace dynamic

} // namespace ptgn