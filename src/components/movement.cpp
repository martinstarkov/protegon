#include "components/movement.h"

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <utility>

#include "common/assert.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/script.h"
#include "core/timer.h"
#include "debug/log.h"
#include "input/input_handler.h"
#include "input/key.h"
#include "math/math.h"
#include "math/vector2.h"
#include "physics/collision/collider.h"
#include "physics/rigid_body.h"

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
	if (Abs(target - current) <= maxDelta) {
		return target;
	}
	return current + Sign(target - current) * maxDelta;
}

} // namespace impl

void MoveWASD(Entity& entity, const V2_float& speed) {
	V2_float position{ GetPosition(entity) };
	MoveWASD(position, speed, false);
	SetPosition(entity, position);
}

void MoveArrowKeys(Entity& entity, const V2_float& speed) {
	V2_float position{ GetPosition(entity) };
	MoveArrowKeys(position, speed, false);
	SetPosition(entity, position);
}

void MoveWASD(V2_float& vel, const V2_float& amount, bool cancel_velocity_if_unpressed) {
	impl::MoveImpl(vel, amount, Key::A, Key::D, Key::W, Key::S, cancel_velocity_if_unpressed);
}

void MoveArrowKeys(V2_float& vel, const V2_float& amount, bool cancel_velocity_if_unpressed) {
	impl::MoveImpl(
		vel, amount, Key::Left, Key::Right, Key::Up, Key::Down, cancel_velocity_if_unpressed
	);
}

void TopDownMovement::Update(Entity& entity, Transform& transform, RigidBody& rb, float dt) {
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
	} else if (only_orthogonal_movement) {
		dir.x = 0.0f;
	}

	if (up_input && !down_input) {
		dir.y = -1.0f;
	} else if (down_input && !up_input) {
		dir.y = 1.0f;
	} else if (only_orthogonal_movement) {
		dir.y = 0.0f;
	}

	if (!dir.IsZero()) {
		facing_direction = dir;
	}

	// Used to flip the character's sprite when she changes direction
	// Also tells us that we are currently pressing a direction button
	if (dir.x != 0.0f) {
		transform.SetScaleX(Abs(transform.GetScale().x) * Sign(dir.x));
	}
	if (flip_vertically && dir.y != 0.0f) {
		transform.SetScaleY(Abs(transform.GetScale().y) * Sign(dir.y));
	}

	// Calculate's the character's desired velocity - which is the direction you are facing,
	// multiplied by the character's maximum speed
	float speed{ std::max(max_speed - friction, 0.0f) };

	V2_float desired_velocity{ dir * speed };

	// Ensure diagonal movement is not faster than axis aligned movement.
	if (desired_velocity.MagnitudeSquared() > speed * speed) {
		desired_velocity = desired_velocity.Normalized() * speed;
	}

	// Calculate movement, depending on whether "Instant Movement" has been checked
	if (use_acceleration) {
		RunWithAcceleration(desired_velocity, rb, dt);
	} else {
		rb.velocity = desired_velocity;
	}

	InvokeCallbacks(entity);

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

template <auto StartFunc, auto ContinueFunc, auto StopFunc>
static void InvokeMoveCallbacks(Scripts& scripts, bool was_moving, bool is_moving) {
	if (!was_moving && is_moving) {
		scripts.AddAction(StartFunc);
	}
	if (is_moving) {
		scripts.AddAction(ContinueFunc);
	}
	if (was_moving && !is_moving) {
		scripts.AddAction(StopFunc);
	}
}

void TopDownMovement::InvokeCallbacks(Entity& entity) {
	if (!entity.Has<Scripts>()) {
		return;
	}

	auto& scripts{ entity.Get<Scripts>() };

	if (dir != prev_dir) {
		// Clamp because turning from left to right can cause a difference in direction of 2.0f,
		// which we see as the same as 1.0f.
		V2_float diff{ Clamp(prev_dir - dir, V2_float{ -1.0f }, V2_float{ 1.0f }) };
		auto dir_state{ GetDirectionState(diff) };
		scripts.AddAction(&PlayerMoveScript::OnDirectionChange, dir_state);
	}

	// TODO: Consider instead of using WasMoving, IsMoving, switch to providing an index and
	// comparison with -1 or 1 or 0.

	InvokeMoveCallbacks<
		&PlayerMoveScript::OnMoveStart, &PlayerMoveScript::OnMove, &PlayerMoveScript::OnMoveStop>(
		scripts, !WasMoving(MoveDirection::None), !IsMoving(MoveDirection::None)
	);
	InvokeMoveCallbacks<
		&PlayerMoveScript::OnMoveUpStart, &PlayerMoveScript::OnMoveUp,
		&PlayerMoveScript::OnMoveUpStop>(
		scripts, WasMoving(MoveDirection::Up), IsMoving(MoveDirection::Up)
	);
	InvokeMoveCallbacks<
		&PlayerMoveScript::OnMoveDownStart, &PlayerMoveScript::OnMoveDown,
		&PlayerMoveScript::OnMoveDownStop>(
		scripts, WasMoving(MoveDirection::Down), IsMoving(MoveDirection::Down)
	);
	InvokeMoveCallbacks<
		&PlayerMoveScript::OnMoveLeftStart, &PlayerMoveScript::OnMoveLeft,
		&PlayerMoveScript::OnMoveLeftStop>(
		scripts, WasMoving(MoveDirection::Left), IsMoving(MoveDirection::Left)
	);
	InvokeMoveCallbacks<
		&PlayerMoveScript::OnMoveRightStart, &PlayerMoveScript::OnMoveRight,
		&PlayerMoveScript::OnMoveRightStop>(
		scripts, WasMoving(MoveDirection::Right), IsMoving(MoveDirection::Right)
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

void TopDownMovement::Move(const V2_float& direction) {
	PTGN_ASSERT(
		!only_orthogonal_movement,
		"Cannot move entity in orthogonal direction unless orthogonal movement is enabled"
	);
	// PTGN_ASSERT(direction.MagnitudeSquared() <= 1.0f, "Move direction must be normalized");
	dir = direction;
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

	set_velocity(0);
	set_velocity(1);
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
		transform.SetScaleX(Abs(transform.GetScale().x) * Sign(dir_x));
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

void PlatformerJump::Ground(
	Entity& entity, const Collision& collision, const CollisionCategory& ground_category
) {
	if (!entity.Has<PlatformerMovement>()) {
		return;
	}

	PTGN_ASSERT((collision.entity.Has<Collider>()));

	bool is_ground_collision{ collision.normal == V2_float{ 0.0f, -1.0f } };

	if (!is_ground_collision) {
		return;
	}

	if (collision.entity.Has<Collider>() &&
		collision.entity.Get<Collider>().IsCategory(ground_category)) {
		entity.Get<PlatformerMovement>().grounded = true;
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
		jump_speed += Abs(rb.velocity.y);
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

	if (NearlyEqual(gravity.y, 0.0f)) {
		rb.gravity = 0.0f;
	} else {
		PTGN_ASSERT(time_to_jump_apex != 0.0f);

		rb.gravity = gravity_multiplier * 2 * jump_height /
					 (time_to_jump_apex * time_to_jump_apex * gravity.y);
	}
	PTGN_ASSERT(!std::isinf(rb.gravity));
}

} // namespace ptgn
