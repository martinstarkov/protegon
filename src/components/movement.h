#pragma once

#include "components/rigid_body.h"
#include "components/transform.h"
#include "math/math.h"
#include "math/vector2.h"

namespace ptgn {

struct MovementData {
	// Gravity.
	float gravity_strength{
		0.0f
	}; // Downwards force (gravity) needed for the desired jumpHeight and jumpTimeToApex.
	float gravity_scale{ 1.0f }; // Strength of the player's gravity as a multiplier of gravity (set
								 // in ProjectSettings/Physics2D). Also the value the player's
								 // rigidbody2D.gravityScale is set to.
	float fall_gravity{ 1.0f };	 // Multiplier to the player's gravityScale when falling.
	float max_fall_speed{
		0.0f
	}; // Maximum fall speed (terminal velocity) of the player when falling.
	float fast_fall_gravity{
		1.0f
	}; // Larger multiplier to the player's gravityScale when they are falling and a downwards input
	   // is pressed. Seen in games such as Celeste, lets the player fall extra fast if they wish.
	float max_fast_fall_speed{
		0.0f
	}; // Maximum fall speed(terminal velocity) of the player when performing a faster fall.

	// Run.
	float run_max_speed{ 0.0f }; // Target speed we want the player to reach.
	float run_acceleration{
		0.0f
	}; // The speed at which our player accelerates to max speed, can be set to runMaxSpeed for
	   // instant acceleration down to 0 for none at all
	float run_accel_amount{
		0.0f
	}; // The actual force (multiplied with speedDiff) applied to the player.
	float run_decceleration{
		0.0f
	}; // The speed at which our player decelerates from their current speed, can be set to
	   // runMaxSpeed for instant deceleration down to 0 for none at all.
	float run_deccel_amount{
		0.0f
	}; // Actual force (multiplied with speedDiff) applied to the player.
	// Multipliers applied to acceleration rate when airborne [0 to 1].
	float accel_in_air{ 1.0f };
	float deccel_in_air{ 1.0f }; // [0 to 1].
	bool conserve_momentum{ true };

	// Jump.
	float jump_height{ 0.0f }; // Height of the player's jump.
	float jump_time_to_apex{
		0.0f
	}; // Time between applying the jump force and reaching the desired jump height. These values
	   // also control the player's gravity and jump force.
	float jump_force{ 0.0f }; // The actual force applied (upwards) to the player when they jump.

	float jump_cut_gravity{
		0.0f
	}; // Multiplier to increase gravity if the player releases thje jump button while still jumping
	float jump_hang_gravity{
		1.0f
	}; // Reduces gravity while close to the apex (desired max height) of the jump [0 to 1].
	float jump_hang_time_threshold{
		0.0f
	}; // Speeds (close to 0) where the player will experience extra "jump hang". The player's
	   // velocity.y is closest to 0 at the jump's apex (think of the gradient of a parabola or
	   // quadratic function)
	float jump_hang_acceleration{ 1.0f }; // Multiplier.
	float jump_hang_max_speed{ 1.0f };	  // Multiplier.

	// Wall jump.
	V2_float
		wall_jump_force; // The actual force (set by us) applied to the player when wall jumping.
	float wall_jump_run_lerp{
		1.0f
	}; // Reduces the effect of player's movement while wall jumping [0 to 1].
	float wall_jump_time{
		1.0f
	}; // Time after wall jumping the player's movement is slowed for [0 to 1.5].
	bool do_turn_on_wall_jump{ false }; // Player will rotate to face wall jumping direction.

	// Slide.
	float slide_speed{ 0.0f };
	float slide_accel{ 0.0f };

	// Assists.
	float coyote_time{
		0.5f
	}; // Grace period after falling off a platform, where you can still jump [0.01 to 0.5].
	float jump_input_buffer_time{
		0.5f
	}; // Grace period after pressing jump where a jump will be automatically performed once the
	   // requirements (eg. being grounded) are met [0.01 to 0.5].

	void Recalculate();
};

struct Movement {
	MovementData data;

	bool facing_right{ false };
	bool jumping{ false };
	bool wall_jumping{ false };
	bool sliding{ false };

	float last_ground_time{ 0.0f };
	float last_wall_time{ 0.0f };
	float last_wall_right_time{ 0.0f };
	float last_wall_left_time{ 0.0f };

	// Jump
	bool jump_cut{ false };
	bool jump_falling{ false };

	// Wall Jump
	float wall_jump_start_time{ 0.0f };
	int last_wall_jump_dir{ 0 };

	V2_int move_input;
	float last_pressed_jump_time{ 0.0f };

	Transform ground_check_point;
	// Size of groundCheck depends on the size of your character generally you want them slightly
	// small than width (for ground) and height (for the wall check).
	V2_float ground_check_size{ 0.49f, 0.03f };
	Transform front_wall_check_point;
	Transform back_wall_check_point;
	V2_float wall_check_size{ 0.5f, 1.0f };

	[[nodiscard]] bool JumpKeyDown() const;
	[[nodiscard]] bool JumpKeyUp() const;
	[[nodiscard]] bool LeftKeyDown() const;
	[[nodiscard]] bool RightKeyDown() const;
	[[nodiscard]] bool UpKeyDown() const;
	[[nodiscard]] bool DownKeyDown() const;

	[[nodiscard]] int HorizontalInputDirection() const {
		bool left{ LeftKeyDown() };
		bool right{ RightKeyDown() };
		if (left && !right) {
			return -1;
		} else if (right && !left) {
			return 1;
		}
		return 0;
	}

	[[nodiscard]] int VerticalInputDirection() const {
		bool up{ UpKeyDown() };
		bool down{ DownKeyDown() };
		if (up && !down) {
			return -1;
		} else if (down && !up) {
			return 1;
		}
		return 0;
	}

	void Init(RigidBody& rb) {
		data.Recalculate();
		SetGravityScale(rb, data.gravity_scale);
		facing_right = true;
	}

	[[nodiscard]] bool OverlappingGround() const;

	[[nodiscard]] bool OverlappingFrontWall() const;

	[[nodiscard]] bool OverlappingBackWall() const;

	void Update(Transform& transform, RigidBody& rb);

	void FixedUpdate(RigidBody& rb) const {
		// Handle Run
		if (wall_jumping) {
			Run(rb, data.wall_jump_run_lerp);
		} else {
			Run(rb, 1);
		}

		// Handle Slide
		if (sliding) {
			Slide(rb);
		}
	}

	void OnJumpInput() {
		last_pressed_jump_time = data.jump_input_buffer_time;
	}

	void OnJumpUpInput(const RigidBody& rb) {
		if (CanJumpCut(rb) || CanWallJumpCut(rb)) {
			jump_cut = true;
		}
	}

	void SetGravityScale(RigidBody& rb, float scale) const {
		rb.gravity = scale;
	}

	void Run(RigidBody& rb, float lerpAmount) const {
		// Calculate the direction we want to move in and our desired velocity
		float targetSpeed = static_cast<float>(move_input.x) * data.run_max_speed;
		// We can reduce are control using Lerp() this smooths changes to are direction and speed
		targetSpeed = Lerp(rb.velocity.x, targetSpeed, lerpAmount);

		float accelRate;

		// Gets an acceleration value based on if we are accelerating (includes turning)
		// or trying to decelerate (stop). As well as applying a multiplier if we're air borne.
		if (last_ground_time > 0) {
			accelRate =
				(FastAbs(targetSpeed) > 0.01f) ? data.run_accel_amount : data.run_deccel_amount;
		} else {
			accelRate = (FastAbs(targetSpeed) > 0.01f)
						  ? data.run_accel_amount * data.accel_in_air
						  : data.run_deccel_amount * data.deccel_in_air;
		}

		// Increase are acceleration and maxSpeed when at the apex of their jump, makes the jump
		// feel a bit more bouncy, responsive and natural
		if ((jumping || wall_jumping || jump_falling) &&
			FastAbs(rb.velocity.y) < data.jump_hang_time_threshold) {
			accelRate	*= data.jump_hang_acceleration;
			targetSpeed *= data.jump_hang_max_speed;
		}
		// We won't slow the player down if they are moving in their desired direction but at a
		// greater speed than their maxSpeed
		if (data.conserve_momentum && FastAbs(rb.velocity.x) > FastAbs(targetSpeed) &&
			Sign(rb.velocity.x) == Sign(targetSpeed) && FastAbs(targetSpeed) > 0.01f &&
			last_ground_time < 0) {
			// Prevent any deceleration from happening, or in other words conserve are current
			// momentum You could experiment with allowing for the player to slightly increae their
			// speed whilst in this "state"
			accelRate = 0;
		}

		// Calculate difference between current velocity and desired velocity
		float speedDif = targetSpeed - rb.velocity.x;
		// Calculate force along x-axis to apply to thr player

		float movement = speedDif * accelRate;

		// Convert this to a vector and apply to rigidbody
		rb.AddAcceleration(movement * V2_float::Right());
	}

	void Turn(Transform& transform) {
		transform.scale.x *= -1;
		facing_right	   = !facing_right;
	}

	void Jump(RigidBody& rb) {
		// Ensures we can't call Jump multiple times from one press
		last_pressed_jump_time = 0;
		last_ground_time	   = 0;

		// We increase the force applied if we are falling
		// This means we'll always feel like we jump the same amount
		//(setting the player's Y velocity to 0 beforehand will likely work the same, but I find
		// this more elegant :D)
		float force = data.jump_force;
		if (rb.velocity.y < 0) {
			force -= rb.velocity.y;
		}

		rb.AddImpulse(V2_float::Up() * force);
	}

	void WallJump(RigidBody& rb, int dir) {
		// Ensures we can't call Wall Jump multiple times from one press
		last_pressed_jump_time = 0;
		last_ground_time	   = 0;
		last_wall_right_time   = 0;
		last_wall_left_time	   = 0;

		V2_float force	= data.wall_jump_force;
		force.x		   *= static_cast<float>(dir); // apply force in opposite direction of wall

		if (Sign(rb.velocity.x) != Sign(force.x)) {
			force.x -= rb.velocity.x;
		}

		if (rb.velocity.y < 0) { // checks whether player is falling, if so we subtract the
								 // velocity.y (counteracting force of gravity). This ensures the
								 // player always reaches our desired jump force or greater
			force.y -= rb.velocity.y;
		}

		// Unlike in the run we want to use the Impulse mode.
		// The default mode will apply are force instantly ignoring masss
		rb.AddImpulse(force);
	}

	void Slide(RigidBody& rb) const;

	void CheckDirectionToFace(Transform& transform, bool isMovingRight) {
		if (isMovingRight != facing_right) {
			Turn(transform);
		}
	}

	[[nodiscard]] bool CanJump() const {
		return last_ground_time > 0 && !jumping;
	}

	[[nodiscard]] bool CanWallJump() const {
		return last_pressed_jump_time > 0 && last_wall_time > 0 && last_ground_time <= 0 &&
			   (!wall_jumping || (last_wall_right_time > 0 && last_wall_jump_dir == 1) ||
				(last_wall_left_time > 0 && last_wall_jump_dir == -1));
	}

	[[nodiscard]] bool CanJumpCut(const RigidBody& rb) const {
		return jumping && rb.velocity.y > 0;
	}

	[[nodiscard]] bool CanWallJumpCut(const RigidBody& rb) const {
		return wall_jumping && rb.velocity.y > 0;
	}

	[[nodiscard]] bool CanSlide() const {
		if (last_wall_time > 0 && !jumping && !wall_jumping && last_ground_time <= 0) {
			return true;
		} else {
			return false;
		}
	}
};

} // namespace ptgn