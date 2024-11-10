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
	int dir_x{ 1 };

	V2_float desired_velocity;
	float max_speed_change;
	float acceleration;
	float deceleration;
	float turn_speed;

	// TODO: Move to PlatformerJump class.
	bool grounded{ false };

	// Parameters:
	// Maximum movement speed
	float max_speed{ 10.0f * 60.0f };
	// How fast to reach max speed
	float max_acceleration{ 52.0f * 60.0f };
	// How fast to stop after letting go
	float max_deceleration{ 52.0f * 60.0f };
	// How fast to stop when changing direction
	float max_turn_speed{ 80.0f * 60.0f };
	// How fast to reach max speed when in mid-air
	float max_air_acceleration{ 52.0f * 60.0f };
	// How fast to stop in mid-air when no direction is used
	float max_air_deceleration{ 52.0f * 60.0f };
	// How fast to stop when changing direction when in mid-air.
	float max_air_turn_speed{ 80.0f * 60.0f };

	// Optional parameters:
	bool use_acceleration{ true };
	float friction{ 0.0f };

	Key left_key{ Key::A };
	Key right_key{ Key::D };

	void Update(Transform& transform, RigidBody& rb);

	[[nodiscard]] static float MoveTowards(float current, float target, float maxDelta);

	void RunWithAcceleration(RigidBody& rb);
};

struct PlatformerJump {
public:
	Key jump_key{ Key::W };
	milliseconds jump_buffer_time{ 250 };
	milliseconds coyote_time{ 150 };

	static void Ground(Collision c, CollisionCategory ground_category);
	static void Unground(Collision c, CollisionCategory ground_category);

	float upward_gravity_multiplier{ 1.0f };
	float downward_gravity_multiplier{ 2.0f };
	bool variable_jump_height{ true };
	float jump_cut_off{ 2.0f };
	float speed_limit{ 60.0f * 600.0f };
	float jump_height{ 360.0f };
	float time_to_jump_apex{ 1.0f };
	bool jumping{ false };

private:
	float default_gravity_scale{ 1.0f };

	Timer jump_buffer_;
	Timer coyote_timer_;

public:
	void Jump(RigidBody& rb);

	void Update(RigidBody& rb, bool grounded);

	void CalculateGravity(RigidBody& rb, bool grounded) const;
};

} // namespace ptgn