#pragma once

#include "core/event/event.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "core/time/timer.h"
#include "platform/input/key.h"
#include "runtime/ecs/entity.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/move_direction.h"
#include "runtime/physics/rigid_body.h"
#include "serialization/json/serialize.h"

namespace ptgn {

class Scene;

namespace event {

struct PlayerMoveStart : public Event<PlayerMoveStart> {
	PlayerMoveStart() = default;

	explicit PlayerMoveStart(MoveDirection direction) : direction{ direction } {}

	MoveDirection direction;		 // Direction at start

	operator MoveDirection() const { // NOSONAR
		return direction;
	}
};

struct PlayerMoveHeld : public Event<PlayerMoveHeld> {
	PlayerMoveHeld() = default;

	explicit PlayerMoveHeld(MoveDirection direction) : direction{ direction } {}

	MoveDirection direction;		 // Current direction

	operator MoveDirection() const { // NOSONAR
		return direction;
	}
};

struct PlayerMoveStop : public Event<PlayerMoveStop> {
	PlayerMoveStop() = default;

	explicit PlayerMoveStop(MoveDirection last_direction) : last_direction{ last_direction } {}

	MoveDirection last_direction;	 // Direction before stopping

	operator MoveDirection() const { // NOSONAR
		return last_direction;
	}
};

struct PlayerMoveDirectionChange : public Event<PlayerMoveDirectionChange> {
	PlayerMoveDirectionChange() = default;

	explicit PlayerMoveDirectionChange(V2_float difference, MoveDirection current_direction) :
		difference{ difference }, current_direction{ current_direction } {}

	V2_float difference;
	MoveDirection current_direction; // Resulting direction

	operator MoveDirection() const { // NOSONAR
		return current_direction;
	}
};

struct PlayerMoveDirectionStart : public Event<PlayerMoveDirectionStart> {
	PlayerMoveDirectionStart() = default;

	explicit PlayerMoveDirectionStart(MoveDirection direction) : direction{ direction } {}

	MoveDirection direction;

	operator MoveDirection() const { // NOSONAR
		return direction;
	}
};

struct PlayerMoveDirectionHeld : public Event<PlayerMoveDirectionHeld> {
	PlayerMoveDirectionHeld() = default;

	explicit PlayerMoveDirectionHeld(MoveDirection direction) : direction{ direction } {}

	MoveDirection direction;

	operator MoveDirection() const { // NOSONAR
		return direction;
	}
};

struct PlayerMoveDirectionStop : public Event<PlayerMoveDirectionStop> {
	PlayerMoveDirectionStop() = default;

	explicit PlayerMoveDirectionStop(MoveDirection direction) : direction{ direction } {}

	MoveDirection direction;

	operator MoveDirection() const { // NOSONAR
		return direction;
	}
};

} // namespace event

namespace impl {

void MoveImpl(
	const Scene& scene, V2_float& vel, V2_float amount, Key left_key, Key right_key, Key up_key,
	Key down_key, bool cancel_velocity_if_unpressed
);

[[nodiscard]] float MoveTowards(float current, float target, float max_delta);

} // namespace impl

void MoveWASD(
	const Scene& scene, V2_float& vel, V2_float amount, bool cancel_velocity_if_unpressed = true
);

void MoveArrowKeys(
	const Scene& scene, V2_float& vel, V2_float amount, bool cancel_velocity_if_unpressed = true
);

void MoveWASD(Entity entity, V2_float speed);

void MoveArrowKeys(Entity entity, V2_float speed);

struct TopDownMovement {
	// Parameters:

	/// @brief Maximum movement speed.
	float max_speed{ 4.0f * 60.0f };
	/// @brief How fast to reach max speed.
	float max_acceleration{ 20.0f * 60.0f };
	/// @brief How fast to stop after letting go.
	float max_deceleration{ 20.0f * 60.0f };
	/// @brief How fast to stop when changing direction.
	float max_turn_speed{ 60.0f * 60.0f };

	float friction{ 0.0f };

	/// @brief If false, velocity will be immediately set to desired velocity. Otherwise integration
	/// is used.
	bool use_acceleration{ true };

	/// @brief If true, flips the player transform scale vertically upon moving up.
	bool flip_vertically{ false };

	/// @brief Whether or not the movement keys cause movement.
	bool keys_enabled{ true };

	/// @brief If true, only permits vertical and horizontal movement.
	bool only_orthogonal_movement{ true };

	Key up_key{ Key::W };
	Key left_key{ Key::A };
	Key down_key{ Key::S };
	Key right_key{ Key::D };

	/// @param dt Unit: seconds.
	void Update(Entity entity, Transform& transform, RigidBody& rb, float dt);

	/// @brief Invoke a movement command in a specific direction the same as a key input would. If
	/// move direction is none, movement inputs will be set to false.
	void Move(MoveDirection direction);

	void Move(V2_float direction);

	/// @return True if the player is moving in the specified direction.
	[[nodiscard]] bool IsMoving(MoveDirection direction) const;

	/// @return True if the player was moving in the specified direction.
	[[nodiscard]] bool WasMoving(MoveDirection direction) const;

	/// @return The current direction of movement.
	MoveDirection GetDirection() const;

	/// @return The previous direction of movement.
	MoveDirection GetPreviousDirection() const;

	V2_float facing_direction;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		TopDownMovement, max_speed, max_acceleration, max_deceleration, max_turn_speed, friction,
		use_acceleration, flip_vertically, keys_enabled, only_orthogonal_movement, up_key, left_key,
		down_key, right_key, facing_direction, up_input, down_input, left_input, right_input, dir,
		prev_dir
	)

private:
	/// @brief @param dt Unit: seconds.
	void RunWithAcceleration(V2_float desired_velocity, RigidBody& rb, float dt) const;

	static bool GetMovingState(V2_float d, MoveDirection direction);

	static MoveDirection GetDirectionState(V2_float d);

	void InvokeCallbacks(Entity entity) const;

	/// @brief Whether or not an input of this type has been given in this frame.
	/// Useful for moving a player without having to press keys.
	bool up_input{ false };
	bool down_input{ false };
	bool left_input{ false };
	bool right_input{ false };

	/// @brief Keep track of movement starting and stopping.
	V2_float dir;
	V2_float prev_dir;
};

struct PlatformerMovement {
	// TODO: Move to PlatformerJump class?
	/// @brief Whether or not the player is currently on the ground. Determines their acceleration
	/// (air or ground) and if they can jump or not.
	bool grounded{ false };

	// Parameters:

	/// @brief Maximum movement speed.
	float max_speed{ 4.0f * 60.0f };
	/// @brief  How fast to reach max speed.
	float max_acceleration{ 20.0f * 60.0f };
	/// @brief  How fast to stop after letting go.
	float max_deceleration{ 20.0f * 60.0f };
	/// @brief  How fast to stop when changing direction.
	float max_turn_speed{ 60.0f * 60.0f };
	/// @brief  How fast to reach max speed when in mid-air.
	float max_air_acceleration{ 40.0f * 60.0f };
	/// @brief  How fast to stop in mid-air when no direction is used.
	float max_air_deceleration{ 40.0f * 60.0f };
	/// @brief  How fast to stop when changing direction when in mid-air.
	float max_air_turn_speed{ 60.0f * 60.0f };

	/// @brief  If false, velocity will be immediately set to desired velocity. Otherwise
	/// integration is used.
	bool use_acceleration{ true };
	float friction{ 0.0f };

	Key left_key{ Key::A };
	Key right_key{ Key::D };

	/// @param dt Unit: seconds.
	void Update(const Scene& scene, Transform& transform, RigidBody& rb, float dt) const;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		PlatformerMovement, grounded, max_speed, max_acceleration, max_deceleration, max_turn_speed,
		max_air_acceleration, max_air_deceleration, max_air_turn_speed, use_acceleration, friction,
		left_key, right_key
	)
private:
	/// @param dt Unit: seconds.
	void RunWithAcceleration(
		const Scene& scene, V2_float desired_velocity, float dir_x, RigidBody& rb, float dt
	) const;
};

struct PlatformerJump {
public:
	void Update(const Scene& scene, RigidBody& rb, bool grounded, V2_float gravity);

	Key jump_key{ Key::W };
	Key down_key{ Key::S };
	/// @brief  Duration of time for which a jump buffer is valid (before hitting the ground).
	milliseconds jump_buffer_time{ 150 };
	/// @brief  Duration of time after leaving the ground for which the player can jump.
	milliseconds coyote_time{ 150 };

	static void Ground(Entity entity, const Collision& collision, ColliderMask ground_mask);

	/// @brief  Gravity when grounded or near zero velocity.
	float default_gravity_scale{ 5.0f };
	/// @brief  Gravity when jumping.
	float upward_gravity_multiplier{ 5.0f };
	/// @brief  Gravity when falling.
	float downward_gravity_multiplier{ 6.0f };
	/// @brief  Gravity when jump key is released before reaching the jump apex.
	float jump_cut_off_gravity_multiplier{ 12.0f };
	/// @brief  Gravity when down key is held.
	float downward_speedup_gravity_multiplier{ 12.0f };
	/// @brief  If player presses down_key, their downward gravity increases.
	bool downward_key_speedup{ true };
	/// @brief  If player lets go of jump key, their downward gravity increases.
	bool variable_jump_height{ true };
	/// @brief  Maximum downward velocity.
	float terminal_velocity{ 36000.0f };
	float jump_height{ 150.0f };
	float time_to_jump_apex{ 1.0f };

	PTGN_SERIALIZER_REGISTER_NAMED(
		PlatformerJump, KeyValue("jump_key", jump_key), KeyValue("down_key", down_key),
		KeyValue("jump_buffer_time", jump_buffer_time), KeyValue("coyote_time", coyote_time),
		KeyValue("default_gravity_scale", default_gravity_scale),
		KeyValue("upward_gravity_multiplier", upward_gravity_multiplier),
		KeyValue("downward_gravity_multiplier", downward_gravity_multiplier),
		KeyValue("jump_cut_off_gravity_multiplier", jump_cut_off_gravity_multiplier),
		KeyValue("downward_speedup_gravity_multiplier", downward_speedup_gravity_multiplier),
		KeyValue("downward_key_speedup", downward_key_speedup),
		KeyValue("variable_jump_height", variable_jump_height),
		KeyValue("terminal_velocity", terminal_velocity), KeyValue("jump_height", jump_height),
		KeyValue("time_to_jump_apex", time_to_jump_apex), KeyValue("jumping", jumping_),
		KeyValue("jump_buffer", jump_buffer_), KeyValue("coyote_timer", coyote_timer_)
	)

private:
	bool jumping_{ false };

	Timer jump_buffer_;
	Timer coyote_timer_;

	void Jump(RigidBody& rb, V2_float gravity);
	void CalculateGravity(const Scene& scene, RigidBody& rb, bool grounded, V2_float gravity) const;
};

} // namespace ptgn