#include "physics/physics.h"

#include "components/common.h"
#include "components/movement.h"
#include "core/game.h"
#include "core/manager.h"
#include "components/transform.h"
#include "math/vector2.h"
#include "physics/rigid_body.h"

namespace ptgn {

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
	for (auto [e, enabled, t, rb, m] :
		 manager.EntitiesWith<Enabled, Transform, RigidBody, TopDownMovement>()) {
		if (!enabled) {
			continue;
		}
		m.Update(t, rb, dt);
	}
	for (auto [e, enabled, t, rb, m, j] :
		 manager.EntitiesWith<Enabled, Transform, RigidBody, PlatformerMovement, PlatformerJump>(
		 )) {
		if (!enabled) {
			continue;
		}
		m.Update(t, rb, dt);
		j.Update(rb, m.grounded, gravity_);
	}
	for (auto [e, enabled, rb] : manager.EntitiesWith<Enabled, RigidBody>()) {
		if (!enabled) {
			continue;
		}
		rb.Update(gravity_, dt);
	}
	for (auto [e, enabled, m] : manager.EntitiesWith<Enabled, PlatformerMovement>()) {
		if (!enabled) {
			continue;
		}
		m.grounded = false;
	}
}

void Physics::PostCollisionUpdate(Manager& manager) const {
	float dt{ Physics::dt() };
	for (auto [e, enabled, t, rb] : manager.EntitiesWith<Enabled, Transform, RigidBody>()) {
		if (!enabled) {
			continue;
		}
		t.position += rb.velocity * dt;
	}
}

} // namespace ptgn
