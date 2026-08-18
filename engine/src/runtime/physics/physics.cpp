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
#include "runtime/physics/platformer_event.h"
#include "runtime/physics/platformer_grounding.h"
#include "runtime/physics/platformer_jump.h"
#include "runtime/physics/platformer_jump_registry.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_event.h"

namespace ptgn {

Physics::Physics(Scene& scene) : scene_{ &scene } {
	RegisterBuiltInPlatformerJumpControllers();
}

void Physics::Rebind(Scene& scene) {
	scene_ = &scene;
}

std::optional<Bounds> Physics::GetBounds() const {
	return bounds_;
}

void Physics::SetBounds(std::optional<Bounds> bounds) {
	PTGN_ASSERT(
		bounds.has_value() ? bounds.value().size.IsPositive() : true,
		"Bounds size cannot be negative"
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
	PTGN_ASSERT(scene_);
	return scene_->ctx().dt();
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

void Physics::UpdatePlatformerGrounding() const {
	PTGN_ASSERT(scene_);

	for (auto [entity, movement] : scene_->EntitiesWith<PlatformerMovement>()) {
		bool was_grounded{ movement.IsGrounded() };
		ptgn::UpdatePlatformerGrounding(
			entity,
			movement.grounding,
			movement.grounding_state_,
			gravity_
		);

		if (!was_grounded && movement.IsGrounded()) {
			PushEvent<event::PlayerGrounded>(
				entity,
				movement.GetGroundEntity(),
				movement.GetGroundNormal()
			);
		} else if (was_grounded && !movement.IsGrounded()) {
			PushEvent<event::PlayerUngrounded>(entity);
		}
	}
}

void Physics::PreCollisionUpdate() const {
	if (!enabled_) {
		return;
	}

	auto frame_dt{ Physics::dt() };
	PTGN_ASSERT(scene_);

	UpdatePlatformerGrounding();

	for (auto [entity, transform, rigid_body, movement] :
		 scene_->EntitiesWith<Transform, RigidBody, TopDownMovement>()) {
		movement.Update(entity, transform, rigid_body, frame_dt);
	}

	for (auto [entity, transform, rigid_body, movement] :
		 scene_->EntitiesWith<Transform, RigidBody, PlatformerMovement>()) {
		movement.Update(*scene_, transform, rigid_body, frame_dt);


		PlatformerJumpContext context{
			.entity = entity,
			.scene = *scene_,
			.platformer = movement,
			.transform = transform,
			.rigid_body = rigid_body,
			.gravity = gravity_,
			.dt = frame_dt,
		};
		UpdatePlatformerJumpController(entity, context);
	}

	for (auto [entity, rigid_body] : scene_->EntitiesWith<RigidBody>()) {
		rigid_body.Update(gravity_, frame_dt);
	}
}

void Physics::PostCollisionUpdate() const {
	if (!enabled_) {
		return;
	}

	auto frame_dt{ Physics::dt() };
	PTGN_ASSERT(scene_);

	for (auto [entity, transform, rigid_body] : scene_->EntitiesWith<Transform, RigidBody>()) {
		transform.Translate(rigid_body.velocity * frame_dt.count());
		transform.Rotate(rigid_body.angular_velocity * frame_dt.count());
		transform.ClampRotation();

		if (!bounds_.has_value()) {
			continue;
		}

		BoundaryBehavior behavior{ bounds_.value().behavior };
		if (entity.Has<BoundaryBehavior>()) {
			behavior = entity.Get<BoundaryBehavior>();
		}

		HandleBoundary(
			transform,
			rigid_body.velocity,
			Bounds{ bounds_.value().position, bounds_.value().size, behavior }
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
			transform.position = Clamp(position, min, max);
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
		default:
			PTGN_ERROR("Unknown BoundaryBehavior: ", std::to_underlying(bounds.behavior));
	}
}

void Physics::Reset() {
	enabled_ = true;
	bounds_ = {};
	gravity_ = {};
}

} // namespace ptgn
