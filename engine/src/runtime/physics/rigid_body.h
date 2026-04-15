#pragma once

#include <optional>

#include "core/math/angle.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "serialization/serialize.h"

namespace ptgn {

class Entity;

struct RigidBody {
	RigidBody() = default;
	RigidBody(float max_speed, float drag, float gravity, bool immovable);

	/// @brief vel += accel * dt
	void AddAcceleration(V2_float acceleration, secondsf dt);

	/// @brief angular_vel += angular_accel * dt
	void AddAngularAcceleration(Radians angular_acceleration, secondsf dt);
	void AddAngularAcceleration(Degrees angular_acceleration, secondsf dt);

	/// @brief vel += impulse
	void AddImpulse(V2_float impulse);

	/// @brief angular_vel += angular_impulse
	void AddAngularImpulse(Radians angular_impulse);
	void AddAngularImpulse(Degrees angular_impulse);

	void Update(V2_float physics_gravity, secondsf dt);

	/// @brief nullopt means no enforcement of maximum speed.
	std::optional<float> max_speed;
	/// @brief nullopt means no enforcement of maximum angular speed.
	std::optional<float> max_angular_speed;

	float drag{ 0.0f };
	float angular_drag{ 0.0f };

	/// @brief Gravity relative to scene.physics.GetGravity().
	float gravity{ 0.0f };

	bool immovable{ false };

	V2_float velocity;
	Radians angular_velocity{ 0.0f };

	PTGN_REFLECT(
		RigidBody, max_speed, max_angular_speed, drag, angular_drag, gravity, immovable, velocity,
		angular_velocity
	)
};

[[nodiscard]] bool IsImmovable(Entity entity, bool check_parents = true);

} // namespace ptgn