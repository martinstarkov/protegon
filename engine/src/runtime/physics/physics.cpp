#include "runtime/physics/physics.h"

#include <chrono>
#include <optional>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/angle.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/time.h"
#include "runtime/ecs/entity.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

namespace ptgn {

Physics::Physics(Scene& scene) : scene_{ scene } {}

std::optional<Bounds> Physics::GetBounds() const {
	return bounds_;
}

void Physics::SetBounds(std::optional<Bounds> bounds) {
	PTGN_ASSERT(
		bounds.has_value() ? bounds->size.IsPositive() : true, "Bounds size cannot be negative"
	);

	bounds_ = bounds;
}

V2_float Physics::GetGravity() const {
	return gravity_;
}

void Physics::SetGravity(V2_float gravity) {
	gravity_ = gravity;
}

secondsf Physics::dt() const {
	return scene_.ctx().dt();
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

bool Physics::IsEnabled() const {
	return enabled_;
}

void Physics::PreCollisionUpdate() const {
	if (!enabled_) {
		return;
	}

	auto dt{ Physics::dt() };

	for (auto [entity, transform, rigid_body, movement] :
		 scene_.EntitiesWith<Transform, RigidBody, TopDownMovement>()) {
		movement.Update(entity, transform, rigid_body, dt);
	}

	for (auto [e, transform, rigid_body, movement, jump] :
		 scene_.EntitiesWith<Transform, RigidBody, PlatformerMovement, PlatformerJump>()) {
		movement.Update(scene_, transform, rigid_body, dt);
		jump.Update(scene_, rigid_body, movement.grounded, gravity_, dt);
	}

	for (auto [e, rigid_body] : scene_.EntitiesWith<RigidBody>()) {
		rigid_body.Update(gravity_, dt);
	}

	for (auto [e, movement] : scene_.EntitiesWith<PlatformerMovement>()) {
		movement.grounded = false;
	}
}

void Physics::PostCollisionUpdate() const {
	if (!enabled_) {
		return;
	}

	auto dt{ Physics::dt() };

	for (auto [entity, transform, rigid_body] : scene_.EntitiesWith<Transform, RigidBody>()) {
		transform.Translate(rigid_body.velocity * dt.count());
		transform.Rotate(rigid_body.angular_velocity * dt.count());
		transform.ClampRotation();

		if (!bounds_.has_value()) {
			continue;
		}

		// Enforce world boundary behavior for the positions.

		BoundaryBehavior behavior{ bounds_->behavior };

		if (entity.Has<BoundaryBehavior>()) {
			behavior = entity.Get<BoundaryBehavior>();
		}

		HandleBoundary(
			transform, rigid_body.velocity, Bounds{ bounds_->position, bounds_->size, behavior }
		);
	}
}

void Physics::HandleBoundary(Transform& transform, V2_float& velocity, const Bounds& bounds) {
	auto position{ transform.position };
	auto half_size{ bounds.size / 2.0f };
	auto min{ bounds.position - half_size };
	auto max{ bounds.position + half_size };

	switch (bounds.behavior) {
		case BoundaryBehavior::StopVelocity: {
			auto clamped{ Clamp(position, min, max) };
			if (clamped != position) {
				velocity = {};
			}
			transform.position = clamped;
			break;
		}
		case BoundaryBehavior::SlideVelocity: {
			auto clamped{ Clamp(position, min, max) };
			transform.position = clamped;
			break;
		}
		case BoundaryBehavior::ReflectVelocity: {
			auto clamped{ Clamp(position, min, max) };
			if (clamped.x != position.x) {
				velocity.x *= -1.0f;
			}
			if (clamped.y != position.y) {
				velocity.y *= -1.0f;
			}
			transform.position = clamped;
			break;
		}
		default: PTGN_ERROR("Unknown physics boundary behavior specified");
	}
}

void Physics::Reset() {
	enabled_ = true;
	bounds_	 = {};
	gravity_ = {};
}

} // namespace ptgn
