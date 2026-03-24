#include "runtime/physics/physics.h"

#include <chrono>
#include <optional>
#include <ostream>
#include <utility>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
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
		bounds.has_value() ? bounds->size.BothAboveZero() : true, "Bounds size cannot be negative"
	);

	bounds_ = bounds;
}

V2_float Physics::GetGravity() const {
	return gravity_;
}

void Physics::SetGravity(V2_float gravity) {
	gravity_ = gravity;
}

float Physics::dt() const {
	return scene_.ctx().dt().count();
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

	float dt{ Physics::dt() };

	for (auto [entity, transform, rigid_body, movement] :
		 scene_.EntitiesWith<Transform, RigidBody, TopDownMovement>()) {
		movement.Update(entity, transform, rigid_body, dt);
	}

	scene_.Refresh();

	for (auto [e, transform, rigid_body, movement, jump] :
		 scene_.EntitiesWith<Transform, RigidBody, PlatformerMovement, PlatformerJump>()) {
		movement.Update(scene_, transform, rigid_body, dt);
		jump.Update(scene_, rigid_body, movement.grounded, gravity_);
	}

	for (auto [e, rigid_body] : scene_.EntitiesWith<RigidBody>()) {
		rigid_body.Update(gravity_, dt);
	}

	for (auto [e, movement] : scene_.EntitiesWith<PlatformerMovement>()) {
		movement.grounded = false;
	}

	scene_.Refresh();
}

void Physics::PostCollisionUpdate() const {
	if (!enabled_) {
		return;
	}

	float dt{ Physics::dt() };

	for (auto [entity, transform, rigid_body] : scene_.EntitiesWith<Transform, RigidBody>()) {
		transform.Translate(rigid_body.velocity * dt);
		transform.Rotate(rigid_body.angular_velocity * dt);
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

	scene_.Refresh();
}

void Physics::HandleBoundary(Transform& transform, V2_float& velocity, const Bounds& bounds) {
	const V2_float position{ transform.GetPosition() };

	auto half_size{ bounds.size / 2.0f };
	auto min_bound{ bounds.position - half_size };
	auto max_bound{ bounds.position + half_size };

	switch (bounds.behavior) {
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

void Physics::Reset() {
	enabled_ = true;
	bounds_	 = {};
	gravity_ = {};
}

std::ostream& operator<<(std::ostream& os, BoundaryBehavior behavior) {
	switch (behavior) {
		using enum BoundaryBehavior;
		case StopVelocity:	  return os << "StopVelocity";
		case SlideVelocity:	  return os << "SlideVelocity";
		case ReflectVelocity: return os << "ReflectVelocity";
		default:			  PTGN_ERROR("Unknown BoundaryBehavior: ", std::to_underlying(behavior));
	}
}

} // namespace ptgn
