#include "components/movement.h"

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <utility>

#include "core/entity.h"
#include "core/game.h"
#include "core/transform.h"
#include "event/input_handler.h"
#include "event/key.h"
#include "math/collision/collider.h"
#include "math/math.h"
#include "math/vector2.h"
#include "physics/rigid_body.h"
#include "utility/assert.h"
#include "utility/log.h"
#include "utility/timer.h"

namespace ptgn {

namespace impl {

void MoveImpl(
	V2_float& vel, const V2_float& amount, Key left_key, Key right_key, Key up_key, Key down_key,
	bool cancel_velocity_if_unpressed
) {
	bool left{ game.input.KeyPressed(left_key) };
	bool right{ game.input.KeyPressed(right_key) };
	bool up{ game.input.KeyPressed(up_key) };
	bool down{ game.input.KeyPressed(down_key) };

	if (left && !right) {
		vel.x -= amount.x;
	} else if (right && !left) {
		vel.x += amount.x;
	}
	if (up && !down) {
		vel.y -= amount.y;
	} else if (down && !up) {
		vel.y += amount.y;
	}

	if (cancel_velocity_if_unpressed && !up && !down && !left && !right) {
		vel = {};
	}
}

float MoveTowards(float current, float target, float maxDelta) {
	if (FastAbs(target - current) <= maxDelta) {
		return target;
	}
	return current + Sign(target - current) * maxDelta;
}

} // namespace impl

void MoveWASD(V2_float& vel, const V2_float& amount, bool cancel_velocity_if_unpressed) {
	impl::MoveImpl(vel, amount, Key::A, Key::D, Key::W, Key::S, cancel_velocity_if_unpressed);
}

void MoveArrowKeys(V2_float& vel, const V2_float& amount, bool cancel_velocity_if_unpressed) {
	impl::MoveImpl(
		vel, amount, Key::LEFT, Key::RIGHT, Key::UP, Key::DOWN, cancel_velocity_if_unpressed
	);
}

void TopDownMovement::Update(Transform& transform, RigidBody& rb, float dt) {
	if (keys_enabled) {
		if (game.input.KeyPressed(up_key)) {
			up_input = true;
		}
		if (game.input.KeyPressed(down_key)) {
			down_input = true;
		}
		if (game.input.KeyPressed(left_key)) {
			left_input = true;
		}
		if (game.input.KeyPressed(right_key)) {
			right_input = true;
		}
	}

	if (left_input && !right_input) {
		dir.x = -1.0f;
	} else if (right_input && !left_input) {
		dir.x = 1.0f;
	} else {
		dir.x = 0.0f;
	}

	if (up_input && !down_input) {
		dir.y = -1.0f;
	} else if (down_input && !up_input) {
		dir.y = 1.0f;
	} else {
		dir.y = 0.0f;
	}

	// Used to flip the character's sprite when she changes direction
	// Also tells us that we are currently pressing a direction button
	if (dir.x != 0.0f) {
		transform.scale.x = FastAbs(transform.scale.x) * Sign(dir.x);
	}
	if (flip_vertically && dir.y != 0.0f) {
		transform.scale.y = FastAbs(transform.scale.y) * Sign(dir.y);
	}

	// Calculate's the character's desired velocity - which is the direction you are facing,
	// multiplied by the character's maximum speed
	float speed{ std::max(max_speed - friction, 0.0f) };

	V2_float desired_velocity{ dir * speed };

	// Ensure diagonal movement is not faster than axis aligned movement.
	if (desired_velocity.MagnitudeSquared() > max_speed * max_speed) {
		desired_velocity = desired_velocity.Normalized() * max_speed;
	}

	// Calculate movement, depending on whether "Instant Movement" has been checked
	if (use_acceleration) {
		RunWithAcceleration(desired_velocity, rb, dt);
	} else {
		rb.velocity = desired_velocity;
	}

	InvokeCallbacks();

	// Cancel inputs for next frame.
	Move(MoveDirection::None);

	prev_dir = dir;
}

MoveDirection TopDownMovement::GetDirectionState(const V2_float& d) {
	// The reason these are nearly equals is because d can be dir - prev_dir.
	if (NearlyEqual(d.x, 0.0f) && NearlyEqual(d.y, 0.0f)) {
		return MoveDirection::None;
	} else if (NearlyEqual(d.x, -1.0f) && NearlyEqual(d.y, 0.0f)) {
		return MoveDirection::Left;
	} else if (NearlyEqual(d.x, 1.0f) && NearlyEqual(d.y, 0.0f)) {
		return MoveDirection::Right;
	} else if (NearlyEqual(d.x, 0.0f) && NearlyEqual(d.y, 1.0f)) {
		return MoveDirection::Down;
	} else if (NearlyEqual(d.x, 0.0f) && NearlyEqual(d.y, -1.0f)) {
		return MoveDirection::Up;
	} else if (NearlyEqual(d.x, 1.0f) && NearlyEqual(d.y, 1.0f)) {
		return MoveDirection::DownRight;
	} else if (NearlyEqual(d.x, -1.0f) && NearlyEqual(d.y, 1.0f)) {
		return MoveDirection::DownLeft;
	} else if (NearlyEqual(d.x, 1.0f) && NearlyEqual(d.y, -1.0f)) {
		return MoveDirection::UpRight;
	} else if (NearlyEqual(d.x, -1.0f) && NearlyEqual(d.y, -1.0f)) {
		return MoveDirection::UpLeft;
	} else {
		PTGN_ERROR("Invalid direction parameter");
	}
}

void TopDownMovement::InvokeCallbacks() {
	auto callbacks = [](bool was_moving, bool is_moving, const auto& start_func,
						const auto& continue_func, const auto& stop_func) {
		if (!was_moving && is_moving && start_func) {
			std::invoke(start_func);
		}
		if (is_moving && continue_func) {
			std::invoke(continue_func);
		}
		if (was_moving && !is_moving && stop_func) {
			std::invoke(stop_func);
		}
	};

	if (dir != prev_dir && on_direction_change) {
		// Clamp because turning from left to right can cause a difference in direction of 2.0f,
		// which we see as the same as 1.0f.
		V2_float diff{ Clamp(dir - prev_dir, V2_float{ -1.0f }, V2_float{ 1.0f }) };
		std::invoke(on_direction_change, GetDirectionState(diff));
	}

	// TODO: Instead of using WasMoving, IsMoving, switch to providing an index and comparison with
	// -1 or 1 or 0.

	std::invoke(
		callbacks, !WasMoving(MoveDirection::None), !IsMoving(MoveDirection::None), on_move_start,
		on_move, on_move_stop
	);
	std::invoke(
		callbacks, WasMoving(MoveDirection::Up), IsMoving(MoveDirection::Up), on_move_up_start,
		on_move_up, on_move_up_stop
	);
	std::invoke(
		callbacks, WasMoving(MoveDirection::Down), IsMoving(MoveDirection::Down),
		on_move_down_start, on_move_down, on_move_down_stop
	);
	std::invoke(
		callbacks, WasMoving(MoveDirection::Left), IsMoving(MoveDirection::Left),
		on_move_left_start, on_move_left, on_move_left_stop
	);
	std::invoke(
		callbacks, WasMoving(MoveDirection::Right), IsMoving(MoveDirection::Right),
		on_move_right_start, on_move_right, on_move_right_stop
	);
}

bool TopDownMovement::GetMovingState(const V2_float& d, MoveDirection direction) {
	switch (direction) {
		case MoveDirection::None:	   return d.x == 0.0f && d.y == 0.0f;
		case MoveDirection::Left:	   return d.x == -1.0f;
		case MoveDirection::Right:	   return d.x == 1.0f;
		case MoveDirection::Up:		   return d.y == -1.0f;
		case MoveDirection::Down:	   return d.y == 1.0f;
		case MoveDirection::UpLeft:	   return d.x == -1.0f && d.y == -1.0f;
		case MoveDirection::UpRight:   return d.x == 1.0f && d.y == -1.0f;
		case MoveDirection::DownLeft:  return d.x == -1.0f && d.y == 1.0f;
		case MoveDirection::DownRight: return d.x == 1.0f && d.y == 1.0f;
		default:					   PTGN_ERROR("Unrecognized movement direction");
	}
}

bool TopDownMovement::IsMoving(MoveDirection direction) const {
	return GetMovingState(dir, direction);
}

bool TopDownMovement::WasMoving(MoveDirection direction) const {
	return GetMovingState(prev_dir, direction);
}

MoveDirection TopDownMovement::GetDirection() const {
	return GetDirectionState(dir);
}

MoveDirection TopDownMovement::GetPreviousDirection() const {
	return GetDirectionState(prev_dir);
}

void TopDownMovement::Move(MoveDirection direction) {
	switch (direction) {
		case MoveDirection::None:
			left_input	= false;
			right_input = false;
			down_input	= false;
			up_input	= false;
			break;
		case MoveDirection::Left:  left_input = true; break;
		case MoveDirection::Right: right_input = true; break;
		case MoveDirection::Up:	   up_input = true; break;
		case MoveDirection::Down:  down_input = true; break;
		case MoveDirection::UpLeft:
			up_input   = true;
			left_input = true;
			break;
		case MoveDirection::UpRight:
			up_input	= true;
			right_input = true;
			break;
		case MoveDirection::DownLeft:
			down_input = true;
			left_input = true;
			break;
		case MoveDirection::DownRight:
			down_input	= true;
			right_input = true;
			break;
		default: PTGN_ERROR("Unrecognized movement direction");
	}
}

void TopDownMovement::RunWithAcceleration(const V2_float& desired_velocity, RigidBody& rb, float dt)
	const {
	// In the future one could include a state machine based choice here.
	float acceleration{ max_acceleration };
	float deceleration{ max_deceleration };
	float turn_speed{ max_turn_speed };

	auto set_velocity = [&](std::size_t i) {
		float max_speed_change{ 0.0f };

		if (dir[i] != 0.0f) {
			// If the sign (i.e. positive or negative) of our input direction doesn't match our
			// movement, it means we're turning around and so should use the turn speed stat.
			if (!NearlyEqual(Sign(dir[i]), Sign(rb.velocity[i]))) {
				max_speed_change = turn_speed * dt;
			} else {
				// If they match, it means we're simply running along and so should use the
				// acceleration stat
				max_speed_change = acceleration * dt;
			}
		} else {
			// And if we're not pressing a direction at all, use the deceleration stat
			max_speed_change = deceleration * dt;
		}

		// Move our velocity towards the desired velocity, at the rate of the number calculated
		// above
		rb.velocity[i] = impl::MoveTowards(rb.velocity[i], desired_velocity[i], max_speed_change);
	};

	std::invoke(set_velocity, 0);
	std::invoke(set_velocity, 1);
}

void PlatformerMovement::Update(Transform& transform, RigidBody& rb, float dt) const {
	bool left{ game.input.KeyPressed(left_key) };
	bool right{ game.input.KeyPressed(right_key) };

	float dir_x{ 0.0f };

	if (left && !right) {
		dir_x = -1.0f;
	}

	if (right && !left) {
		dir_x = 1.0f;
	}

	// Used to flip the character's sprite when she changes direction
	// Also tells us that we are currently pressing a direction button
	if (dir_x != 0.0f) {
		transform.scale.x = FastAbs(transform.scale.x) * Sign(dir_x);
	}

	// Calculate's the character's desired velocity - which is the direction you are facing,
	// multiplied by the character's maximum speed
	V2_float desired_velocity{ dir_x * std::max(max_speed - friction, 0.0f), 0.0f };

	// Calculate movement, depending on whether "Instant Movement" has been checked
	if (use_acceleration) {
		RunWithAcceleration(desired_velocity, dir_x, rb, dt);
	} else {
		if (grounded) {
			rb.velocity.x = desired_velocity.x;
		} else {
			RunWithAcceleration(desired_velocity, dir_x, rb, dt);
		}
	}
}

void PlatformerMovement::RunWithAcceleration(
	const V2_float& desired_velocity, float dir_x, RigidBody& rb, float dt
) const {
	// Set our acceleration, deceleration, and turn speed stats, based on whether we're on the
	// ground on in the air

	float acceleration{ grounded ? max_acceleration : max_air_acceleration };
	float deceleration{ grounded ? max_deceleration : max_air_deceleration };
	float turn_speed{ grounded ? max_turn_speed : max_air_turn_speed };

	bool left{ game.input.KeyPressed(left_key) };
	bool right{ game.input.KeyPressed(right_key) };
	bool pressing_key{ (left && !right) || (!left && right) };

	float max_speed_change{ 0.0f };

	if (pressing_key) {
		// If the sign (i.e. positive or negative) of our input direction doesn't match our
		// movement, it means we're turning around and so should use the turn speed stat.
		if (!NearlyEqual(Sign(dir_x), Sign(rb.velocity.x))) {
			max_speed_change = turn_speed * dt;
		} else {
			// If they match, it means we're simply running along and so should use the
			// acceleration stat
			max_speed_change = acceleration * dt;
		}
	} else {
		// And if we're not pressing a direction at all, use the deceleration stat
		max_speed_change = deceleration * dt;
	}

	// Move our velocity towards the desired velocity, at the rate of the number calculated
	// above
	rb.velocity.x = impl::MoveTowards(rb.velocity.x, desired_velocity.x, max_speed_change);
}

void PlatformerJump::Ground(const Collision& c, const CollisionCategory& ground_category) {
	PTGN_ASSERT((c.entity2.HasAny<BoxCollider, CircleCollider>()));
	if ((c.entity2.Has<BoxCollider>() && c.entity2.Get<BoxCollider>().IsCategory(ground_category)
		) ||
		(c.entity2.Has<CircleCollider>() &&
		 c.entity2.Get<CircleCollider>().IsCategory(ground_category))) {
		if (c.entity1.Has<PlatformerMovement>() && c.normal == V2_float{ 0.0f, -1.0f }) {
			c.entity1.Get<PlatformerMovement>().grounded = true;
		}
	}
}

void PlatformerJump::Update(RigidBody& rb, bool grounded, const V2_float& gravity) {
	bool pressed_jump{ game.input.KeyDown(jump_key) };

	if (grounded) {
		coyote_timer_.Start();
		jumping_ = false;
	}

	if (pressed_jump && !grounded) {
		// Player desires to jump but currently cant.
		jump_buffer_.Start();
	}

	bool jump_buffered{ jump_buffer_.IsRunning() && !jump_buffer_.Completed(jump_buffer_time) };
	bool in_coyote{ coyote_timer_.IsRunning() && !coyote_timer_.Completed(coyote_time) };

	CalculateGravity(rb, grounded, gravity);

	// Situations where pressing jump triggers a jump:
	// 1. On ground.
	// 2. During coyote time.
	// 3. During jump buffer time.

	if ((pressed_jump && grounded) || (grounded && jump_buffered) ||
		(pressed_jump && in_coyote && !grounded)) {
		Jump(rb, gravity);
	}
}

void PlatformerJump::Jump(RigidBody& rb, const V2_float& gravity) {
	jumping_ = true;

	jump_buffer_.Stop();
	coyote_timer_.Stop();

	// If we have double jump on, allow us to jump again (but only once)
	// canJumpAgain = (maxAirJumps == 1 && canJumpAgain == false);

	// Determine the power of the jump, based on our gravity and stats
	float jump_speed{ std::sqrt(2.0f * gravity.y * rb.gravity * jump_height) };

	// If Kit is moving up or down when she jumps (such as when doing a double jump), change
	// the jump_speed; This will ensure the jump is the exact same strength, no matter your
	// velocity.
	if (rb.velocity.y < 0.0f) {
		jump_speed = std::max(jump_speed - rb.velocity.y, 0.0f);
	} else if (rb.velocity.y > 0.0f) {
		jump_speed += FastAbs(rb.velocity.y);
	}

	rb.velocity.y -= jump_speed;

	// if (juice != null) {
	//	// Apply the jumping effects on the juice script
	//	juice.jumpEffects();
	// }
}

void PlatformerJump::CalculateGravity(RigidBody& rb, bool grounded, const V2_float& gravity) const {
	float gravity_multiplier{ 0.0f };

	if (grounded) {
		gravity_multiplier = default_gravity_scale;
	} else if (downward_key_speedup && game.input.KeyPressed(down_key)) {
		gravity_multiplier = downward_speedup_gravity_multiplier;
	} else if (rb.velocity.y < -0.01f) {
		if (!variable_jump_height ||
			(variable_jump_height && game.input.KeyPressed(jump_key) && jumping_)) {
			gravity_multiplier = upward_gravity_multiplier;
		} else if (variable_jump_height) {
			gravity_multiplier = jump_cut_off_gravity_multiplier;
		}
	} else if (rb.velocity.y > 0.01f) {
		gravity_multiplier = downward_gravity_multiplier;
	} else {
		gravity_multiplier = default_gravity_scale;
	}

	if (rb.velocity.y > 0) {
		rb.velocity.y = std::clamp(rb.velocity.y, 0.0f, terminal_velocity);
	}
	// TODO: Incorporate rb gravity.
	rb.gravity =
		gravity_multiplier * 2 * jump_height / (time_to_jump_apex * time_to_jump_apex * gravity.y);
}

} // namespace ptgn
