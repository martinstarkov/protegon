#include "components/movement.h"

#include <algorithm>
#include <utility>

#include "core/game.h"
#include "event/input_handler.h"
#include "event/key.h"
#include "math/math.h"
#include "math/vector2.h"
#include "physics/physics.h"
#include "rigid_body.h"
#include "transform.h"
#include "utility/debug.h"

namespace ptgn {

void MovementData::Recalculate() {
	gravity_strength = -(2.0f * jump_height) / (jump_time_to_apex * jump_time_to_apex);

	float gravity_y = game.physics.GetGravity().y;
	if (gravity_y != 0.0f) {
		gravity_scale = gravity_strength / gravity_y;
	} else {
		gravity_scale = 0.0f;
	}

	PTGN_ASSERT(game.physics.dt() > 0.0f);
	float inv_dt{ 1.0f / game.physics.dt() };
	run_accel_amount  = run_acceleration / run_max_speed * inv_dt;
	run_deccel_amount = run_decceleration / run_max_speed * inv_dt;

	jump_force = FastAbs(gravity_strength) * jump_time_to_apex;

	run_acceleration  = std::clamp(run_acceleration, 0.01f, run_max_speed);
	run_decceleration = std::clamp(run_decceleration, 0.01f, run_max_speed);
}

bool Movement::JumpKeyDown() const {
	return game.input.KeyDown(Key::SPACE);
}

bool Movement::JumpKeyUp() const {
	return game.input.KeyDown(Key::SPACE);
}

bool Movement::LeftKeyDown() const {
	return game.input.KeyDown(Key::A);
}

bool Movement::RightKeyDown() const {
	return game.input.KeyDown(Key::D);
}

bool Movement::UpKeyDown() const {
	return game.input.KeyDown(Key::W);
}

bool Movement::DownKeyDown() const {
	return game.input.KeyDown(Key::S);
}

bool Movement::OverlappingGround() const {
	// TODO: Add ground collision check.
	// return OverlapBox(ground_check_point.position, ground_check_size);
	return game.input.KeyPressed(Key::G);
}

bool Movement::OverlappingFrontWall() const {
	// TODO: Add wall collision check.
	// return OverlapBox(front_wall_check_point.position, wall_check_size);
	return game.input.KeyPressed(Key::F);
}

bool Movement::OverlappingBackWall() const {
	// TODO: Add wall collision check.
	// return OverlapBox(back_wall_check_point.position, wall_check_size);
	return game.input.KeyPressed(Key::B);
}

void Movement::Slide(RigidBody& rb) const {
	// Works the same as the Run but only in the y-axis
	// THis seems to work fine, buit maybe you'll find a better way to implement a slide into
	// this system
	float speedDif = data.slide_speed - rb.velocity.y;
	float movement = speedDif * data.slide_accel;
	// So, we clamp the movement here to prevent any over corrections (these aren't noticeable
	// in the Run) The force applied can't be greater than the (negative) speedDifference * by
	// how many times a second FixedUpdate() is called. For more info research how force are
	// applied to rigidbodies.
	float inv_dt{ 1.0f / game.physics.dt() };

	movement = std::clamp(movement, -FastAbs(speedDif) * inv_dt, FastAbs(speedDif) * inv_dt);

	rb.AddAcceleration(movement * V2_float::Up());
}

void Movement::Update(Transform& transform, RigidBody& rb) {
	float dt{ game.physics.dt() };
	float time{ game.time() };

	last_ground_time	   -= dt;
	last_wall_time		   -= dt;
	last_wall_right_time   -= dt;
	last_wall_left_time	   -= dt;
	last_pressed_jump_time -= dt;

	move_input.x = HorizontalInputDirection();
	move_input.y = VerticalInputDirection();

	if (move_input.x != 0) {
		CheckDirectionToFace(transform, move_input.x > 0);
	}

	if (JumpKeyDown()) {
		OnJumpInput();
	}

	if (JumpKeyUp()) {
		OnJumpUpInput(rb);
	}

	if (!jumping) {
		// Ground Check
		if (OverlappingGround() && !jumping)	 // checks if set box overlaps with ground
		{
			last_ground_time = data.coyote_time; // if so sets the lastGrounded to coyoteTime
		}

		// Right Wall Check
		if (((OverlappingFrontWall() && facing_right) || (OverlappingBackWall() && !facing_right)
			) &&
			!wall_jumping) {
			last_wall_right_time = data.coyote_time;
		}

		// Right Wall Check
		if (((OverlappingFrontWall() && !facing_right) || (OverlappingBackWall() && facing_right)
			) &&
			!wall_jumping) {
			last_wall_left_time = data.coyote_time;
		}

		// Two checks needed for both left and right walls since whenever the play turns the
		// wall checkPoints swap sides
		last_wall_time = std::max(last_wall_left_time, last_wall_right_time);
	}

	if (jumping && rb.velocity.y < 0) {
		jumping = false;

		if (!wall_jumping) {
			jump_falling = true;
		}
	}

	if (wall_jumping && time - wall_jump_start_time > data.wall_jump_time) {
		wall_jumping = false;
	}

	if (last_ground_time > 0 && !jumping && !wall_jumping) {
		jump_cut = false;

		if (!jumping) {
			jump_falling = false;
		}
	}

	// Jump
	if (CanJump() && last_pressed_jump_time > 0) {
		jumping		 = true;
		wall_jumping = false;
		jump_cut	 = false;
		jump_falling = false;
		Jump(rb);
	}
	// WALL JUMP
	else if (CanWallJump() && last_pressed_jump_time > 0) {
		wall_jumping		 = true;
		jumping				 = false;
		jump_cut			 = false;
		jump_falling		 = false;
		wall_jump_start_time = time;
		last_wall_jump_dir	 = (last_wall_right_time > 0) ? -1 : 1;

		WallJump(rb, last_wall_jump_dir);
	}

	// Slide checks.
	if (CanSlide() && ((last_wall_left_time > 0 && move_input.x < 0) ||
					   (last_wall_right_time > 0 && move_input.x > 0))) {
		sliding = true;
	} else {
		sliding = false;
	}

	// Gravity.
	// Higher gravity if we've released the jump input or are falling
	if (sliding) {
		SetGravityScale(rb, 0);
	} else if (rb.velocity.y < 0 && move_input.y < 0) {
		// Much higher gravity if holding down
		SetGravityScale(rb, data.gravity_scale * data.fast_fall_gravity);
		// Caps maximum fall speed, so when falling over large distances we don't accelerate to
		// insanely high speeds
		rb.velocity.y = std::max(rb.velocity.y, -data.max_fast_fall_speed);
	} else if (jump_cut) {
		// Higher gravity if jump button released
		SetGravityScale(rb, data.gravity_scale * data.jump_cut_gravity);
		rb.velocity.y = std::max(rb.velocity.y, -data.max_fall_speed);
	} else if ((jumping || wall_jumping || jump_falling) && FastAbs(rb.velocity.y) < data.jump_hang_time_threshold) {
		SetGravityScale(rb, data.gravity_scale * data.jump_hang_gravity);
	} else if (rb.velocity.y < 0) {
		// Higher gravity if falling
		SetGravityScale(rb, data.gravity_scale * data.fall_gravity);
		// Caps maximum fall speed, so when falling over large distances we don't accelerate to
		// insanely high speeds
		rb.velocity.y = std::max(rb.velocity.y, -data.max_fall_speed);
	} else {
		// Default gravity if standing on a platform or moving upwards
		SetGravityScale(rb, data.gravity_scale);
	}

	FixedUpdate(rb);
}

} // namespace ptgn