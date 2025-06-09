#include "physics/physics.h"

#include "components/common.h"
#include "components/movement.h"
#include "components/transform.h"
#include "core/game.h"
#include "core/manager.h"
#include "math/vector2.h"
#include "physics/rigid_body.h"

namespace ptgn {

V2_float Physics::GetBoundsTopLeft() const {
	return bounds_top_left_;
}

V2_float Physics::GetBoundsSize() const {
	return bounds_size_;
}

void Physics::SetBounds(const V2_float& top_left_position, const V2_float& size) {
	PTGN_ASSERT(size.x >= 0.0f);
	PTGN_ASSERT(size.y >= 0.0f);

	bounds_top_left_ = top_left_position;
	bounds_size_	 = size;
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

void Physics::PreCollisionUpdate(Manager& manager) const {
	float dt{ Physics::dt() };

	for (auto [entity, enabled, transform, rigid_body, movement] :
		 manager.EntitiesWith<Enabled, Transform, RigidBody, TopDownMovement>()) {
		if (!enabled) {
			continue;
		}
		movement.Update(entity, transform, rigid_body, dt);
	}

	for (auto [e, enabled, transform, rigid_body, movement, jump] :
		 manager.EntitiesWith<Enabled, Transform, RigidBody, PlatformerMovement, PlatformerJump>(
		 )) {
		if (!enabled) {
			continue;
		}
		movement.Update(transform, rigid_body, dt);
		jump.Update(rigid_body, movement.grounded, gravity_);
	}

	for (auto [e, enabled, rigid_body] : manager.EntitiesWith<Enabled, RigidBody>()) {
		if (!enabled) {
			continue;
		}
		rigid_body.Update(gravity_, dt);
	}

	for (auto [e, enabled, movement] : manager.EntitiesWith<Enabled, PlatformerMovement>()) {
		if (!enabled) {
			continue;
		}
		movement.grounded = false;
	}

	manager.Refresh();
}

void Physics::PostCollisionUpdate(Manager& manager) const {
	float dt{ Physics::dt() };

	V2_float min_bounds{ bounds_top_left_ };
	V2_float max_bounds{ bounds_top_left_ + bounds_size_ };

	bool enforce_bounds{ !bounds_size_.IsZero() };

	for (auto [entity, enabled, transform, rigid_body] :
		 manager.EntitiesWith<Enabled, Transform, RigidBody>()) {
		if (!enabled) {
			continue;
		}

		transform.position += rigid_body.velocity * dt;

		if (!enforce_bounds) {
			continue;
		}

		// Enforce rough world bounds.

		if (transform.position.x < min_bounds.x) {
			transform.position.x -= transform.position.x - min_bounds.x;
		} else if (transform.position.x > max_bounds.x) {
			transform.position.x += max_bounds.x - transform.position.x;
		}

		if (transform.position.y < min_bounds.y) {
			transform.position.y -= transform.position.y - min_bounds.y;
		} else if (transform.position.y > max_bounds.y) {
			transform.position.y += max_bounds.y - transform.position.y;
		}
	}

	manager.Refresh();
}

} // namespace ptgn
