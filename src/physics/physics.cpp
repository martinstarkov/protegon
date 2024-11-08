#include "physics/physics.h"

#include "collision/collision.h"
#include "components/transform.h"
#include "core/game.h"
#include "ecs/ecs.h"
#include "physics/movement.h"
#include "physics/rigid_body.h"

namespace ptgn {

namespace impl {

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

void Physics::Update(ecs::Manager& manager) const {
	for (auto [e, t, rb, m, j] :
		 manager.EntitiesWith<Transform, RigidBody, PlatformerMovement, PlatformerJump>()) {
		m.Update(t, rb);
		j.Update(rb, m.onGround);
	}
	for (auto [e, rb] : manager.EntitiesWith<RigidBody>()) {
		rb.Update();
	}
	game.collision.Update(manager);
	for (auto [e, t, rb] : manager.EntitiesWith<Transform, RigidBody>()) {
		t.position += rb.velocity * game.physics.dt();
	}
}

} // namespace impl

} // namespace ptgn
