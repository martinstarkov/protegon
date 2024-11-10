#pragma once

#include "collision/collider.h"
#include "components/transform.h"
#include "event/key.h"
#include "math/vector2.h"
#include "physics/rigid_body.h"
#include "utility/time.h"
#include "utility/timer.h"

// TODO: Move functions to cpp file.

namespace ptgn {

namespace impl {

void MoveImpl(
	V2_float& vel, const V2_float& amount, Key left_key, Key right_key, Key up_key, Key down_key,
	bool cancel_velocity_if_unpressed
);

} // namespace impl

void MoveWASD(V2_float& vel, const V2_float& amount, bool cancel_velocity_if_unpressed = true);

void MoveArrowKeys(V2_float& vel, const V2_float& amount, bool cancel_velocity_if_unpressed = true);

struct PlatformerMovement {
	// TODO: Move to PlatformerJump class.

	// Whether or not the player is currently on the ground. Determines their acceleration (air or
	// ground) and if they can jump or not.
	bool grounded{ false };

	// Parameters:

	// Maximum movement speed.
	float max_speed{ 4.0f * 60.0f };
	// How fast to reach max speed.
	float max_acceleration{ 20.0f * 60.0f };
	// How fast to stop after letting go.
	float max_deceleration{ 20.0f * 60.0f };
	// How fast to stop when changing direction.
	float max_turn_speed{ 60.0f * 60.0f };
	// How fast to reach max speed when in mid-air.
	float max_air_acceleration{ 40.0f * 60.0f };
	// How fast to stop in mid-air when no direction is used.
	float max_air_deceleration{ 40.0f * 60.0f };
	// How fast to stop when changing direction when in mid-air.
	float max_air_turn_speed{ 60.0f * 60.0f };

	// If false, velocity will be immediately set to desired velocity. Otherwise integration is
	// used.
	bool use_acceleration{ true };
	float friction{ 0.0f };

	Key left_key{ Key::A };
	Key right_key{ Key::D };

	void Update(Transform& transform, RigidBody& rb);

private:
	[[nodiscard]] static float MoveTowards(float current, float target, float maxDelta);

	void RunWithAcceleration(const V2_float& desired_velocity, float dir_x, RigidBody& rb) const;
};

struct PlatformerJump {
public:
	void Update(RigidBody& rb, bool grounded);

	Key jump_key{ Key::W };
	Key down_key{ Key::S };
	// Duration of time for which a jump buffer is valid (before hitting the ground).
	milliseconds jump_buffer_time{ 150 };
	// Duration of time after leaving the ground for which the player can jump.
	milliseconds coyote_time{ 150 };

	static void Ground(Collision c, CollisionCategory ground_category);

	// Gravity when grounded or near zero velocity.
	float default_gravity_scale{ 5.0f };
	// Gravity when jumping.
	float upward_gravity_multiplier{ 5.0f };
	// Gravity when falling.
	float downward_gravity_multiplier{ 6.0f };
	// Gravity when jump key is released before reaching the jump apex.
	float jump_cut_off_gravity_multiplier{ 12.0f };
	// Gravity when down key is held.
	float downward_speedup_gravity_multiplier{ 18.0f };
	// If player presses down_key, their downward gravity increases.
	bool downward_key_speedup{ true };
	// If player lets go of jump key, their downward gravity increases.
	bool variable_jump_height{ true };
	// Maximum downward velocity.
	float terminal_velocity{ 36000.0f };
	float jump_height{ 150.0f };
	float time_to_jump_apex{ 1.0f };

private:
	bool jumping_{ false };

	Timer jump_buffer_;
	Timer coyote_timer_;

	void Jump(RigidBody& rb);
	void CalculateGravity(RigidBody& rb, bool grounded) const;
};

} // namespace ptgn