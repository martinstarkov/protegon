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
	math::Vector2<T> normal;
	// Penetration of objects into each other along the collision normal.
	math::Vector2<T> penetration;
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

// TODO: Implement LineCapsule dynamic.

// TODO: Implement LineCircle dynamic.

// TODO: Implement LineLine dynamic.

// TODO: Implement PointAABB dynamic.

// TODO: Implement PointCapsule dynamic.

// TODO: Implement PointCircle dynamic.

} // namespace dynamic

} // namespace ptgn