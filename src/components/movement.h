#pragma once

#include <functional>
#include <iosfwd>

#include "components/transform.h"
#include "core/time.h"
#include "core/timer.h"
#include "debug/log.h"
#include "events/key.h"
#include "math/vector2.h"
#include "physics/collision/collider.h"
#include "physics/rigid_body.h"

namespace ptgn {

enum class MoveDirection {
	Up,
	Right,
	Down,
	Left,
	UpLeft,
	UpRight,
	DownRight,
	DownLeft,
	None
};

inline std::ostream& operator<<(std::ostream& os, MoveDirection direction) {
	switch (direction) {
		case MoveDirection::UpLeft:	   os << "Up Left"; break;
		case MoveDirection::Up:		   os << "Up"; break;
		case MoveDirection::UpRight:   os << "Up Right"; break;
		case MoveDirection::Left:	   os << "Left"; break;
		case MoveDirection::None:	   os << "None"; break;
		case MoveDirection::Right:	   os << "Right"; break;
		case MoveDirection::DownLeft:  os << "Down Left"; break;
		case MoveDirection::Down:	   os << "Down"; break;
		case MoveDirection::DownRight: os << "Down Right"; break;
		default:					   PTGN_ERROR("Invalid movement direction");
	}

	return os;
}

namespace impl {

void MoveImpl(
	V2_float& vel, const V2_float& amount, Key left_key, Key right_key, Key up_key, Key down_key,
	bool cancel_velocity_if_unpressed
);

[[nodiscard]] float MoveTowards(float current, float target, float max_delta);

} // namespace impl

void MoveWASD(V2_float& vel, const V2_float& amount, bool cancel_velocity_if_unpressed = true);

void MoveArrowKeys(V2_float& vel, const V2_float& amount, bool cancel_velocity_if_unpressed = true);

struct TopDownMovement {
	// Parameters:

	// Maximum movement speed.
	float max_speed{ 4.0f * 60.0f };
	// How fast to reach max speed.
	float max_acceleration{ 20.0f * 60.0f };
	// How fast to stop after letting go.
	float max_deceleration{ 20.0f * 60.0f };
	// How fast to stop when changing direction.
	float max_turn_speed{ 60.0f * 60.0f };

	float friction{ 0.0f };

	// If false, velocity will be immediately set to desired velocity. Otherwise integration is
	// used.
	bool use_acceleration{ true };

	// If true, flips the player transform scale vertically upon moving up.
	bool flip_vertically{ false };

	// Whether or not the movement keys cause movement.
	bool keys_enabled{ true };

	// If true, only permits vertical and horizontal movement.
	bool only_orthogonal_movement{ true };

	Key up_key{ Key::W };
	Key left_key{ Key::A };
	Key down_key{ Key::S };
	Key right_key{ Key::D };

	// TODO: Turn all these callbacks into optional script components.

	// Called every frame that the player is moving.
	std::function<void()> on_move;
	// Called on the first frame of player movement.
	std::function<void()> on_move_start;
	// Called on the first frame of player stopping their movement.
	std::function<void()> on_move_stop;
	// Called when the movement direction changes. Passed parameter is the difference in direction.
	// If not moving, this is simply the new direction. If moving already, this is the newly added
	// component of movement. To get the current direction instead, simply use GetDirection().
	std::function<void(MoveDirection direction_difference)> on_direction_change;

	std::function<void()> on_move_up;
	std::function<void()> on_move_down;
	std::function<void()> on_move_left;
	std::function<void()> on_move_right;

	std::function<void()> on_move_up_start;
	std::function<void()> on_move_down_start;
	std::function<void()> on_move_left_start;
	std::function<void()> on_move_right_start;

	std::function<void()> on_move_up_stop;
	std::function<void()> on_move_down_stop;
	std::function<void()> on_move_left_stop;
	std::function<void()> on_move_right_stop;

	// @param dt Unit: seconds.
	void Update(Transform& transform, RigidBody& rb, float dt);

	// Invoke a movement command in a specific direction the same as a key input would. If move
	// direction is none, movement inputs will be set to false.
	void Move(MoveDirection direction);

	void Move(const V2_float& direction);

	// @return True if the player is moving in the specified direction.
	[[nodiscard]] bool IsMoving(MoveDirection direction) const;

	// @return True if the player was moving in the specified direction.
	[[nodiscard]] bool WasMoving(MoveDirection direction) const;

	// @return The current direction of movement.
	[[nodiscard]] MoveDirection GetDirection() const;

	// @return The previous direction of movement.
	[[nodiscard]] MoveDirection GetPreviousDirection() const;

	V2_float facing_direction;

private:
	// @param dt Unit: seconds.
	void RunWithAcceleration(const V2_float& desired_velocity, RigidBody& rb, float dt) const;

	[[nodiscard]] static bool GetMovingState(const V2_float& d, MoveDirection direction);

	[[nodiscard]] static MoveDirection GetDirectionState(const V2_float& d);

	void InvokeCallbacks();

	// Whether or not an input of this type has been given in this frame.
	// Useful for moving a player without having to press keys.
	bool up_input{ false };
	bool down_input{ false };
	bool left_input{ false };
	bool right_input{ false };
	// Keep track of movement starting and stopping.
	V2_float dir;
	V2_float prev_dir;
};

struct PlatformerMovement {
	// Whether or not the player is currently on the ground. Determines their acceleration (air or
	// ground) and if they can jump or not.
	// TODO: Move to PlatformerJump class?
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

	// @param dt Unit: seconds.
	void Update(Transform& transform, RigidBody& rb, float dt) const;

private:
	// @param dt Unit: seconds.
	void RunWithAcceleration(const V2_float& desired_velocity, float dir_x, RigidBody& rb, float dt)
		const;
};

struct PlatformerJump {
public:
	void Update(RigidBody& rb, bool grounded, const V2_float& gravity);

	Key jump_key{ Key::W };
	Key down_key{ Key::S };
	// Duration of time for which a jump buffer is valid (before hitting the ground).
	milliseconds jump_buffer_time{ 150 };
	// Duration of time after leaving the ground for which the player can jump.
	milliseconds coyote_time{ 150 };

	static void Ground(const Collision& c, const CollisionCategory& ground_category);

	// Gravity when grounded or near zero velocity.
	float default_gravity_scale{ 5.0f };
	// Gravity when jumping.
	float upward_gravity_multiplier{ 5.0f };
	// Gravity when falling.
	float downward_gravity_multiplier{ 6.0f };
	// Gravity when jump key is released before reaching the jump apex.
	float jump_cut_off_gravity_multiplier{ 12.0f };
	// Gravity when down key is held.
	float downward_speedup_gravity_multiplier{ 12.0f };
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

	void Jump(RigidBody& rb, const V2_float& gravity);
	void CalculateGravity(RigidBody& rb, bool grounded, const V2_float& gravity) const;
};

} // namespace ptgn