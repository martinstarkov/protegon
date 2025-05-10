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

	V2_float min_bounds{ bounds_top_left_ };
	V2_float max_bounds{ bounds_top_left_ + bounds_size_ };

	bool enforce_bounds{ !bounds_size_.IsZero() };

	for (auto [e, enabled, t, rb] : manager.EntitiesWith<Enabled, Transform, RigidBody>()) {
		if (!enabled) {
			continue;
		}

		t.position += rb.velocity * dt;

		if (!enforce_bounds) {
			continue;
		}

		// Enforce rough world bounds.

		if (t.position.x < min_bounds.x) {
			t.position.x -= t.position.x - min_bounds.x;
		} else if (t.position.x > max_bounds.x) {
			t.position.x += max_bounds.x - t.position.x;
		}

		if (t.position.y < min_bounds.y) {
			t.position.y -= t.position.y - min_bounds.y;
		} else if (t.position.y > max_bounds.y) {
			t.position.y += max_bounds.y - t.position.y;
		}
	}
}

} // namespace ptgn
