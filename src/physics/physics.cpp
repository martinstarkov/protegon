#include "physics/physics.h"

#include "components/movement.h"
#include "components/transform.h"
#include "core/game.h"
#include "core/manager.h"
#include "math/vector2.h"
#include "physics/rigid_body.h"
#include "scene/scene.h"

namespace ptgn {

V2_float Physics::GetBoundsTopLeft() const {
	return bounds_top_left_;
}

V2_float Physics::GetBoundsSize() const {
	return bounds_size_;
}

void Physics::SetBounds(
	const V2_float& top_left_position, const V2_float& size, BoundaryBehavior behavior
) {
	PTGN_ASSERT(size.x >= 0.0f);
	PTGN_ASSERT(size.y >= 0.0f);

	bounds_top_left_   = top_left_position;
	bounds_size_	   = size;
	boundary_behavior_ = behavior;
}

V2_float Physics::GetGravity() const {
	return gravity_;
}

void Physics::SetGravity(const V2_float& gravity) {
	gravity_ = gravity;
}

float Physics::dt() const {
	// TODO: Consider changing in the future.
	return game.dt();
}

void Physics::SetEnabled(bool enabled) {
	enabled_ = enabled;
}

void Physics::Disable() {
	SetEnabled(false);
}

void Physics::Enable() {
	SetEnabled(true);
}

[[nodiscard]] bool Physics::AreEnabled() const {
	return enabled_;
}

void Physics::PreCollisionUpdate(Scene& scene) const {
	if (!enabled_) {
		return;
	}

	float dt{ Physics::dt() };

	for (auto [entity, transform, rigid_body, movement] :
		 scene.InternalEntitiesWith<Transform, RigidBody, TopDownMovement>()) {
		movement.Update(entity, transform, rigid_body, dt);
	}

	for (auto [e, transform, rigid_body, movement, jump] :
		 scene.InternalEntitiesWith<Transform, RigidBody, PlatformerMovement, PlatformerJump>()) {
		movement.Update(transform, rigid_body, dt);
		jump.Update(rigid_body, movement.grounded, gravity_);
	}

	for (auto [e, rigid_body] : scene.EntitiesWith<RigidBody>()) {
		rigid_body.Update(gravity_, dt);
	}

	for (auto [e, movement] : scene.EntitiesWith<PlatformerMovement>()) {
		movement.grounded = false;
	}

	scene.Refresh();
}

void Physics::PostCollisionUpdate(Scene& scene) const {
	if (!enabled_) {
		return;
	}

	float dt{ Physics::dt() };

	V2_float min_bounds{ bounds_top_left_ };
	V2_float max_bounds{ bounds_top_left_ + bounds_size_ };

	bool enforce_bounds{ !bounds_size_.IsZero() };

	for (auto [entity, transform, rigid_body] :
		 scene.InternalEntitiesWith<Transform, RigidBody>()) {
		transform.position += rigid_body.velocity * dt;
		transform.rotation += rigid_body.angular_velocity * dt;
		transform.rotation	= ClampAngle2Pi(transform.rotation);

		if (!enforce_bounds) {
			continue;
		}

		// Enforce world boundary behavior for the positions.

		HandleBoundary(
			transform.position.x, rigid_body.velocity.x, min_bounds.x, max_bounds.x,
			boundary_behavior_
		);
		HandleBoundary(
			transform.position.y, rigid_body.velocity.y, min_bounds.y, max_bounds.y,
			boundary_behavior_
		);
	}

	scene.Refresh();
}

void Physics::HandleBoundary(
	float& position, float& velocity, float min_bound, float max_bound, BoundaryBehavior behavior
) {
	switch (behavior) {
		case BoundaryBehavior::StopAtBounds:
			if (position < min_bound) {
				position = min_bound;
			} else if (position > max_bound) {
				position = max_bound;
			}
			break;

		case BoundaryBehavior::ReflectVelocity:
			if (position < min_bound) {
				position  = min_bound;
				velocity *= -1.0f;
			} else if (position > max_bound) {
				position  = max_bound;
				velocity *= -1.0f;
			}
			break;

		default: PTGN_ERROR("Unknown physics boundary behavior specified");
	}
}

} // namespace ptgn
