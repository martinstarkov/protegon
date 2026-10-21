#include "runtime/scripting/scripts/physics_scripts.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numbers>
#include <optional>
#include <ranges>
#include <span>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <utility>

#include "app/application_context.h"
#include "app/project.h"
#include "core/graphics/fill_style.h"
#include "core/log.h"
#include "core/math/angle.h"
#include "core/math/geometry/arc.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/ellipse.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/rounded_rect.h"
#include "core/math/geometry/triangle.h"
#include "core/math/math_utils.h"
#include "core/math/noise.h"
#include "core/math/rng.h"
#include "core/math/tolerance.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/offsets.h"
#include "runtime/animation/scripted_animation.h"
#include "runtime/audio/audio_system.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/fx/effect_registry.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/fx/light.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/sprite_stack.h"
#include "runtime/graphics/text/text.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/interaction/draggable.h"
#include "runtime/interaction/dropzone.h"
#include "runtime/interaction/interactive.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/lifetime.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/physics.h"
#include "runtime/physics/platformer_jump.h"
#include "runtime/physics/platformer_jump_registry.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_file.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scene/scene_registry.h"
#include "runtime/scene/scene_transitions.h"
#include "runtime/scripting/script_common.h"
#include "runtime/ui/button.h"
#include "runtime/ui/dialogue.h"
#include "runtime/ui/dropdown.h"
#include "runtime/ui/slider.h"
#include "runtime/ui/toggle_button.h"
#include "runtime/ui/tooltip.h"
#include "runtime/world/tilemap.h"

namespace ptgn {

void RigidBodySetVelocityScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (!target.Has<RigidBody>()) {
		return;
	}
	RigidBody& body{ target.Get<RigidBody>() };
	body.velocity = vector;
}

void RigidBodyAddVelocityScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (!target.Has<RigidBody>()) {
		return;
	}
	RigidBody& body{ target.Get<RigidBody>() };
	body.velocity += vector;
}

void RigidBodyAddImpulseScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (!target.Has<RigidBody>()) {
		return;
	}
	RigidBody& body{ target.Get<RigidBody>() };
	body.AddImpulse(vector);
}

void RigidBodyAddAccelerationScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (!target.Has<RigidBody>()) {
		return;
	}
	RigidBody& body{ target.Get<RigidBody>() };
	body.AddAcceleration(vector, secondsf{ number });
}

void RigidBodyStopMovementScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (!target.Has<RigidBody>()) {
		return;
	}
	RigidBody& body{ target.Get<RigidBody>() };
	body.velocity = {};
}

void RigidBodySetGravityScaleScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (!target.Has<RigidBody>()) {
		return;
	}
	RigidBody& body{ target.Get<RigidBody>() };
	body.gravity = number;
}

void RigidBodySetDragScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (!target.Has<RigidBody>()) {
		return;
	}
	RigidBody& body{ target.Get<RigidBody>() };
	body.drag = number;
}

void RigidBodySetAngularVelocityScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (!target.Has<RigidBody>()) {
		return;
	}
	RigidBody& body{ target.Get<RigidBody>() };
	body.angular_velocity = Degrees{ number }.ToRad();
}

void RigidBodyAddAngularImpulseScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (!target.Has<RigidBody>()) {
		return;
	}
	RigidBody& body{ target.Get<RigidBody>() };
	body.AddAngularImpulse(Degrees{ number });
}

void RigidBodySetMaximumSpeedScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (!target.Has<RigidBody>()) {
		return;
	}
	RigidBody& body{ target.Get<RigidBody>() };
	body.max_speed = option ? std::optional<float>{ number } : std::nullopt;
}

void RigidBodySetImmovableScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (!target.Has<RigidBody>()) {
		return;
	}
	RigidBody& body{ target.Get<RigidBody>() };
	body.immovable = option;
}

void ColliderSetCollisionModeScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (!target.Has<Collider>()) {
		return;
	}
	Collider& collider{ target.Get<Collider>() };
	if (auto mode{ magic_enum::enum_cast<CollisionMode>(text) }) {
		collider.SetCollisionMode(*mode);
	}
}

void ColliderSetColliderShapeScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (!target.Has<Collider>()) {
		return;
	}
	Collider& collider{ target.Get<Collider>() };
	collider.shape = collider_shape;
}

void ColliderSetCollisionResponseScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (!target.Has<Collider>()) {
		return;
	}
	Collider& collider{ target.Get<Collider>() };
	if (auto response{ magic_enum::enum_cast<CollisionResponse>(text) }) {
		collider.response = *response;
	}
}

void ColliderSetCollisionFilterScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (!target.Has<Collider>()) {
		return;
	}
	Collider& collider{ target.Get<Collider>() };
	collider.collides_with = entity_filter;
}

void ColliderSetOverlapModeScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (!target.Has<Collider>()) {
		return;
	}
	Collider& collider{ target.Get<Collider>() };
	collider.SetOverlapMode();
}

void TopDownSetMaximumSpeedScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (auto* movement{ target.TryGet<TopDownMovement>() }) {
		movement->max_speed = number;
	}
}

void TopDownSetAccelerationScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (auto* movement{ target.TryGet<TopDownMovement>() }) {
		movement->max_acceleration = number;
	}
}

void TopDownMoveDirectionScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (auto* movement{ target.TryGet<TopDownMovement>() }) {
		movement->Move(vector);
	}
}

void PlatformerSetMaximumSpeedScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (auto* movement{ target.TryGet<PlatformerMovement>() }) {
		movement->max_speed = number;
	}
}

void PlatformerSetJumpControllerScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	SetPlatformerJumpController(target, text);
}

void JumpJumpScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (auto* jump{ target.TryGet<PlatformerJump>() }) {
		jump->RequestJump();
	}
}

void JumpSetJumpHeightScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (auto* jump{ target.TryGet<PlatformerJump>() }) {
		jump->jump_height = number;
	}
}

void JumpSetGravityMultipliersScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (auto* jump{ target.TryGet<PlatformerJump>() }) {
		jump->upward_gravity_multiplier	  = number;
		jump->downward_gravity_multiplier = number2;
	}
}

void JumpSetAirJumpsScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (auto* jump{ target.TryGet<PlatformerJump>() }) {
		jump->air_jumps = AirJumps{ static_cast<std::uint32_t>(std::max(0, index)) };
	}
}

void PhysicsSetGravityScript::OnStart() {
	auto& physics{ GetScene().ctx().physics };
	physics.SetGravity(vector);
}

void PhysicsEnablePhysicsScript::OnStart() {
	auto& physics{ GetScene().ctx().physics };
	physics.Enable();
}

void PhysicsDisablePhysicsScript::OnStart() {
	auto& physics{ GetScene().ctx().physics };
	physics.Disable();
}

void PhysicsSetBoundsScript::OnStart() {
	auto& physics{ GetScene().ctx().physics };
	auto behavior{ magic_enum::enum_cast<BoundaryBehavior>(text) };
	physics.SetBounds(
		Bounds{ vector, vector2, behavior.value_or(BoundaryBehavior::SlideVelocity) }
	);
}

void PhysicsClearBoundsScript::OnStart() {
	auto& physics{ GetScene().ctx().physics };
	physics.SetBounds();
}

} // namespace ptgn
