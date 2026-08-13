#include "runtime/physics/rigid_body.h"

#include <algorithm>
#include <chrono>
#include <optional>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/angle.h"
#include "core/math/vector2.h"
#include "core/util/time.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"

namespace ptgn {

void RigidBody::Update(V2_float physics_gravity, secondsf dt) {
	velocity += gravity * physics_gravity * dt.count();
	velocity *= 1.0f / (1.0f + drag * dt.count());
	// Or alternatively: velocity *= Clamp01(1.0f - drag * dt);
	angular_velocity *= 1.0f / (1.0f + angular_drag * dt.count());
	if (max_speed.has_value()) {
		PTGN_ASSERT(max_speed.value() >= 0.0f, "Max speed must be a positive number");
		velocity = ClampMagnitude(velocity, -max_speed.value(), max_speed.value());
	}
	if (max_angular_speed.has_value()) {
		PTGN_ASSERT(
			max_angular_speed.value() >= 0.0f, "Max angular speed must be a positive number"
		);
		angular_velocity = Radians{ std::clamp(
			angular_velocity.value, -max_angular_speed.value(), max_angular_speed.value()
		) };
	}
}

RigidBody::RigidBody(float rb_max_speed, float rb_drag, float rb_gravity, bool rb_immovable) :
	max_speed{ rb_max_speed }, drag{ rb_drag }, gravity{ rb_gravity }, immovable{ rb_immovable } {}

void RigidBody::AddAcceleration(V2_float acceleration, secondsf dt) {
	velocity += acceleration * dt.count();
}

void RigidBody::AddAngularAcceleration(Radians angular_acceleration, secondsf dt) {
	angular_velocity += angular_acceleration * dt.count();
}

void RigidBody::AddAngularAcceleration(Degrees angular_acceleration, secondsf dt) {
	AddAngularAcceleration(angular_acceleration.ToRad(), dt);
}

void RigidBody::AddImpulse(V2_float impulse) {
	velocity += impulse;
}

void RigidBody::AddAngularImpulse(Radians angular_impulse) {
	angular_velocity += angular_impulse;
}

void RigidBody::AddAngularImpulse(Degrees angular_impulse) {
	AddAngularImpulse(angular_impulse.ToRad());
}

bool IsImmovable(Entity entity, bool check_parents) {
	if (entity.Has<RigidBody>() && entity.Get<RigidBody>().immovable) {
		return true;
	}

	if (!check_parents) {
		return false;
	}

	bool immovable{ false };

	ForEachParent(
		entity, [](Entity e) { return e.Has<impl::IgnoreParentImmovable>(); },
		[&immovable](Entity parent) {
			if (parent.Has<RigidBody>() && parent.Get<RigidBody>().immovable) {
				immovable = true;
				return false;
			}

			return true;
		}
	);

	return immovable;
}

void IgnoreParentImmovable(Entity entity, bool ignore_parent_immovable) {
	if (ignore_parent_immovable) {
		entity.Add<impl::IgnoreParentImmovable>();
	} else {
		entity.Remove<impl::IgnoreParentImmovable>();
	}
}

} // namespace ptgn