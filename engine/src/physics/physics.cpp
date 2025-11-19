#include "physics/physics.h"

#include "core/app/application.h"
#include "core/app/manager.h"
#include "core/assert.h"
#include "ecs/components/movement.h"
#include "ecs/components/transform.h"
#include "ecs/entity.h"
#include "core/log.h"
#include "math/vector2.h"
#include "physics/rigid_body.h"
#include "world/scene/scene.h"

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
	// return Application::Get().dt();
	// TODO: fix.
	return 1.0f / 60.0f;
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

	// TODO: Fix.
	/*for (auto [entity, transform, rigid_body, movement] :
		 scene.InternalEntitiesWith<Transform, RigidBody, TopDownMovement>()) {
		movement.Update(entity, transform, rigid_body, dt);
	}

	for (auto [entity, movement, scripts] :
		 scene.InternalEntitiesWith<TopDownMovement, Scripts>()) {
		scripts.InvokeActions();
	}*/

	scene.Refresh();

	// TODO: Fix.
	/*for (auto [e, transform, rigid_body, movement, jump] :
		 scene.InternalEntitiesWith<Transform, RigidBody, PlatformerMovement, PlatformerJump>()) {
		movement.Update(transform, rigid_body, dt);
		jump.Update(rigid_body, movement.grounded, gravity_);
	}*/

	for (auto [e, rigid_body] : scene.EntitiesWith<RigidBody>()) {
		rigid_body.Update(gravity_, dt);
	}

	// TODO: Fix.
	/*for (auto [e, movement] : scene.EntitiesWith<PlatformerMovement>()) {
		movement.grounded = false;
	}*/

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
		transform.Translate(rigid_body.velocity * dt);
		transform.Rotate(rigid_body.angular_velocity * dt);
		transform.ClampRotation();

		if (!enforce_bounds) {
			continue;
		}

		// Enforce world boundary behavior for the positions.

		BoundaryBehavior behavior{ boundary_behavior_ };

		if (entity.Has<BoundaryBehavior>()) {
			behavior = entity.Get<BoundaryBehavior>();
		}

		HandleBoundary(transform, rigid_body.velocity, min_bounds, max_bounds, behavior);
	}

	scene.Refresh();
}

void Physics::HandleBoundary(
	Transform& transform, V2_float& velocity, const V2_float& min_bound, const V2_float& max_bound,
	BoundaryBehavior behavior
) {
	const V2_float position{ transform.GetPosition() };
	switch (behavior) {
		case BoundaryBehavior::StopVelocity: {
			V2_float clamped_position{ Clamp(position, min_bound, max_bound) };
			if (clamped_position != position) {
				velocity = {};
			}
			transform.SetPosition(clamped_position);
			break;
		}
		case BoundaryBehavior::SlideVelocity: {
			V2_float clamped_position{ Clamp(position, min_bound, max_bound) };
			transform.SetPosition(clamped_position);
			break;
		}
		case BoundaryBehavior::ReflectVelocity: {
			V2_float clamped_position{ Clamp(position, min_bound, max_bound) };
			if (clamped_position.x != position.x) {
				velocity.x *= -1.0f;
			}
			if (clamped_position.y != position.y) {
				velocity.y *= -1.0f;
			}
			transform.SetPosition(clamped_position);
			break;
		}
		default: PTGN_ERROR("Unknown physics boundary behavior specified");
	}
}

} // namespace ptgn
