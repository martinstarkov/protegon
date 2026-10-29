#include "runtime/physics/physics.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/angle.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/time.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/group_stack.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/collision_event.h"
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

void Physics::UpdatePlatformerGrounding() {
	PTGN_ASSERT(scene_);

	for (auto [entity, movement] : scene_->EntitiesWith<PlatformerMovement>()) {
		if (!scene_->ctx().chunks.ShouldSimulate(entity)) {
			continue;
		}
		bool was_grounded{ movement.IsGrounded() };
		ptgn::UpdatePlatformerGrounding(
			entity, movement.grounding, movement.grounding_state_, gravity_
		);

		if (!was_grounded && movement.IsGrounded()) {
			PushEvent<event::PlayerGrounded>(
				entity, movement.GetGroundEntity(), movement.GetGroundNormal()
			);
			float impact_speed{ 0.0f };
			if (auto* body{ entity.TryGet<RigidBody>() }) {
				impact_speed = std::max(0.0f, body->velocity.Dot(-movement.GetGroundNormal()));
			}

			PushEvent<event::PlayerLanded>(
				entity, movement.GetGroundEntity(), movement.GetGroundNormal(), impact_speed
			);
		} else if (was_grounded && !movement.IsGrounded()) {
			PushEvent<event::PlayerUngrounded>(entity);
		}
	}
}

void Physics::PreCollisionUpdate() {
	if (!enabled_) {
		return;
	}

	auto frame_dt{ Physics::dt() };
	PTGN_ASSERT(scene_);

	UpdatePlatformerGrounding();

	for (auto [entity, transform, rigid_body, movement] :
		 scene_->EntitiesWith<Transform, RigidBody, TopDownMovement>()) {
		if (!IsGroupStackActive(entity) || !scene_->ctx().chunks.ShouldSimulate(entity)) {
			continue;
		}
		movement.Update(entity, transform, rigid_body, frame_dt);
	}

	for (auto [entity, transform, rigid_body, movement] :
		 scene_->EntitiesWith<Transform, RigidBody, PlatformerMovement>()) {
		if (!IsGroupStackActive(entity) || !scene_->ctx().chunks.ShouldSimulate(entity)) {
			continue;
		}
		movement.Update(*scene_, transform, rigid_body, frame_dt);

		PlatformerJumpContext context{
			.entity		= entity,
			.scene		= *scene_,
			.platformer = movement,
			.transform	= transform,
			.rigid_body = rigid_body,
			.gravity	= gravity_,
			.dt			= frame_dt,
		};
		UpdatePlatformerJumpController(entity, context);
	}

	for (auto [entity, rigid_body] : scene_->EntitiesWith<RigidBody>()) {
		if (!IsGroupStackActive(entity) || !scene_->ctx().chunks.ShouldSimulate(entity)) {
			continue;
		}
		rigid_body.Update(gravity_, frame_dt);
	}

	// Detect the apex as velocity changes from opposing gravity to following it.
	if (!gravity_.IsZero()) {
		V2_float down{ gravity_.Normalized() };

		for (auto [entity, movement, body] :
			 scene_->EntitiesWith<PlatformerMovement, RigidBody>()) {
			if (!IsGroupStackActive(entity) || !scene_->ctx().chunks.ShouldSimulate(entity)) {
				continue;
			}

			std::size_t id{ entity.GetECSId() };
			float speed_down{ body.velocity.Dot(down) };
			auto previous{ previous_gravity_velocity_.find(id) };

			if (previous != previous_gravity_velocity_.end() && previous->second < 0.0f &&
				speed_down >= 0.0f && !movement.IsGrounded()) {
				PushEvent<event::PlayerJumpApex>(entity, GetWorldPosition(entity));
			}

			previous_gravity_velocity_[id] = speed_down;
		}
	}
}

void Physics::PostCollisionUpdate() {
	if (!enabled_) {
		return;
	}

	auto dt{ Physics::dt() };
	PTGN_ASSERT(scene_);

	for (auto [entity, transform, rigid_body] : scene_->EntitiesWith<Transform, RigidBody>()) {
		if (!IsGroupStackActive(entity) || !scene_->ctx().chunks.ShouldSimulate(entity)) {
			continue;
		}
		transform.Translate(rigid_body.velocity * dt.count());
		transform.Rotate(rigid_body.angular_velocity * dt.count());
		transform.ClampRotation();

		if (!bounds_.has_value()) {
			continue;
		}

		BoundaryBehavior behavior{ bounds_.value().behavior };
		if (entity.Has<BoundaryBehavior>()) {
			behavior = entity.Get<BoundaryBehavior>();
		}

		V2_float attempted{ transform.position };
		V2_float half_size{ bounds_.value().size * 0.5f };
		V2_float minimum{ bounds_.value().position - half_size };
		V2_float maximum{ bounds_.value().position + half_size };
		bool inside{ attempted.x >= minimum.x && attempted.x <= maximum.x &&
					 attempted.y >= minimum.y && attempted.y <= maximum.y };
		std::size_t id{ entity.GetECSId() };
		auto previous_inside{ previous_inside_bounds_.find(id) };

		if (previous_inside != previous_inside_bounds_.end() && inside != previous_inside->second) {
			if (inside) {
				PushEvent<event::EntityEnteredBounds>(entity, attempted);
			} else {
				PushEvent<event::EntityExitedBounds>(entity, attempted);
			}
		}

		previous_inside_bounds_[id] = inside;

		HandleBoundary(
			transform, rigid_body.velocity,
			Bounds{ bounds_.value().position, bounds_.value().size, behavior }
		);

		if (attempted != transform.position) {
			PushEvent<event::EntityHitBounds>(entity, attempted, transform.position);
		}
	}

	for (auto [entity, movement, collider] : scene_->EntitiesWith<PlatformerMovement, Collider>()) {
		if (!IsGroupStackActive(entity) || !scene_->ctx().chunks.ShouldSimulate(entity)) {
			continue;
		}
		(void)movement;
		Entity wall{};
		V2_float wall_normal{};
		V2_float up{ gravity_.IsZero() ? V2_float{ 0.0f, -1.0f } : -gravity_.Normalized() };
		auto check_contact = [&](const CollisionInfo& hit) {
			float alignment{ hit.normal.Dot(up) };
			if (alignment < -0.7f) {
				PushEvent<event::PlayerHitCeiling>(entity, hit.entity, hit.normal);
			} else if (std::abs(alignment) < 0.5f) {
				wall		= hit.entity;
				wall_normal = hit.normal;
			}
		};
		for (const auto& hit : collider.GetIntersections()) {
			check_contact(hit);
		}
		for (const auto& hit : collider.GetSweeps()) {
			check_contact(hit);
		}
		std::size_t id{ entity.GetECSId() };
		Entity previous{ previous_wall_contacts_[id] };
		if (wall != previous) {
			if (previous) {
				PushEvent<event::PlayerWallContactStop>(entity, previous);
			}
			if (wall) {
				PushEvent<event::PlayerWallContactStart>(entity, wall, wall_normal);
			}
			previous_wall_contacts_[id] = wall;
		}
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
		default: PTGN_ERROR("Unknown BoundaryBehavior: ", std::to_underlying(bounds.behavior));
	}
}

void Physics::Reset() {
	previous_gravity_velocity_.clear();
	previous_wall_contacts_.clear();
	previous_inside_bounds_.clear();
	enabled_ = true;
	bounds_	 = {};
	gravity_ = {};
}

} // namespace ptgn
