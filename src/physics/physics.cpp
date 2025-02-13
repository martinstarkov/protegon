#include "physics/physics.h"

#include "components/transform.h"
#include "core/game.h"
#include "ecs/ecs.h"
#include "math/vector2.h"
#include "physics/movement.h"
#include "physics/rigid_body.h"

namespace ptgn::impl {

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

void Physics::PreCollisionUpdate(ecs::Manager& manager) const {
	float dt{ Physics::dt() };
	for (auto [e, t, rb, m] : manager.EntitiesWith<Transform, RigidBody, TopDownMovement>()) {
		m.Update(t, rb, dt);
	}
	for (auto [e, t, rb, m, j] :
		 manager.EntitiesWith<Transform, RigidBody, PlatformerMovement, PlatformerJump>()) {
		m.Update(t, rb, dt);
		j.Update(rb, m.grounded, gravity_);
	}
	for (auto [e, rb] : manager.EntitiesWith<RigidBody>()) {
		rb.Update(gravity_, dt);
	}
	for (auto [e, m] : manager.EntitiesWith<PlatformerMovement>()) {
		m.grounded = false;
	}
}

void Physics::PostCollisionUpdate(ecs::Manager& manager) const {
	float dt{ Physics::dt() };
	for (auto [e, t, rb] : manager.EntitiesWith<Transform, RigidBody>()) {
		t.position += rb.velocity * dt;
	}
}

} // namespace ptgn::impl
