#include "runtime/physics/movement.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <utility>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/math_utils.h"
#include "core/math/tolerance.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "core/time/timer.h"
#include "platform/input/key.h"
#include "runtime/ecs/entity.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/move_direction.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scripting/scripts.h"

namespace ptgn {

namespace impl {

void MoveImpl(
	const Scene& scene, V2_float& vel, V2_float amount, Key left_key, Key right_key, Key up_key,
	Key down_key, bool cancel_velocity_if_unpressed
) {
	bool left{ scene.ctx().input.KeyHeld(left_key) };
	bool right{ scene.ctx().input.KeyHeld(right_key) };
	bool up{ scene.ctx().input.KeyHeld(up_key) };
	bool down{ scene.ctx().input.KeyHeld(down_key) };

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
	if (std::abs(target - current) <= maxDelta) {
		return target;
	}
	return current + Sign(target - current) * maxDelta;
}

} // namespace impl

void MoveWASD(
	const Scene& scene, V2_float& vel, V2_float amount, bool cancel_velocity_if_unpressed
) {
	impl::MoveImpl(
		scene, vel, amount, Key::A, Key::D, Key::W, Key::S, cancel_velocity_if_unpressed
	);
}

void MoveArrowKeys(
	const Scene& scene, V2_float& vel, V2_float amount, bool cancel_velocity_if_unpressed
) {
	impl::MoveImpl(
		scene, vel, amount, Key::Left, Key::Right, Key::Up, Key::Down, cancel_velocity_if_unpressed
	);
}

void MoveWASD(Entity entity, V2_float speed) {
	V2_float position{ GetPosition(entity) };
	MoveWASD(entity.GetScene(), position, speed, false);
	SetPosition(entity, position);
}

void MoveArrowKeys(Entity entity, V2_float speed) {
	V2_float position{ GetPosition(entity) };
	MoveArrowKeys(entity.GetScene(), position, speed, false);
	SetPosition(entity, position);
}

void TopDownMovement::Update(Entity entity, Transform& transform, RigidBody& rb, secondsf dt) {
	const auto& input{ entity.GetScene().ctx().input };

	if (keys_enabled) {
		if (input.KeyHeld(up_key)) {
			up_input = true;
		}
		if (input.KeyHeld(down_key)) {
			down_input = true;
		}
		if (input.KeyHeld(left_key)) {
			left_input = true;
		}
		if (input.KeyHeld(right_key)) {
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
		transform.SetScaleX(std::abs(transform.GetScale().x) * Sign(dir.x));
	}
	if (flip_vertically && dir.y != 0.0f) {
		transform.SetScaleY(std::abs(transform.GetScale().y) * Sign(dir.y));
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

MoveDirection TopDownMovement::GetDirectionState(V2_float d) {
	using enum MoveDirection;
	// The reason these are nearly equals is because d can be dir - prev_dir.
	if (NearlyEqual(d.x, 0.0f) && NearlyEqual(d.y, 0.0f)) {
		return None;
	} else if (NearlyEqual(d.x, -1.0f) && NearlyEqual(d.y, 0.0f)) {
		return Left;
	} else if (NearlyEqual(d.x, 1.0f) && NearlyEqual(d.y, 0.0f)) {
		return Right;
	} else if (NearlyEqual(d.x, 0.0f) && NearlyEqual(d.y, 1.0f)) {
		return Down;
	} else if (NearlyEqual(d.x, 0.0f) && NearlyEqual(d.y, -1.0f)) {
		return Up;
	} else if (NearlyEqual(d.x, 1.0f) && NearlyEqual(d.y, 1.0f)) {
		return DownRight;
	} else if (NearlyEqual(d.x, -1.0f) && NearlyEqual(d.y, 1.0f)) {
		return DownLeft;
	} else if (NearlyEqual(d.x, 1.0f) && NearlyEqual(d.y, -1.0f)) {
		return UpRight;
	} else if (NearlyEqual(d.x, -1.0f) && NearlyEqual(d.y, -1.0f)) {
		return UpLeft;
	} else {
		PTGN_ERROR("Invalid direction parameter");
	}
}

template <typename StartEvent, typename ContinueEvent, typename StopEvent, typename... Args>
static void InvokeMoveCallbacks(Entity entity, bool was_moving, bool is_moving, Args&&... args) {
	if (!was_moving && is_moving) {
		PushEvent<StartEvent>(entity, std::forward<Args>(args)...);
	}

	if (is_moving) {
		PushEvent<ContinueEvent>(entity, std::forward<Args>(args)...);
	}

	if (was_moving && !is_moving) {
		PushEvent<StopEvent>(entity, std::forward<Args>(args)...);
	}
}

void TopDownMovement::InvokeCallbacks(Entity entity) const {
	using enum MoveDirection;

	if (!entity.Has<impl::Scripts>()) {
		return;
	}

	if (dir != prev_dir) {
		// Clamp because turning from left to right can cause a difference in direction of 2.0f,
		// which we see as the same as 1.0f.
		V2_float diff{ Clamp(prev_dir - dir, V2_float{ -1.0f }, V2_float{ 1.0f }) };
		auto dir_state{ GetDirectionState(diff) };
		PushEvent<event::PlayerMoveDirectionChange>(entity, diff, dir_state);
	}

	// TODO: Consider instead of using WasMoving, IsMoving, switch to providing an index and
	// comparison with -1 or 1 or 0.

	// Generic movements.
	auto was_moving{ !WasMoving(None) };
	auto is_moving{ !IsMoving(None) };

	if (!was_moving && is_moving) {
		PushEvent<event::PlayerMoveStart>(entity, GetDirection());
	}

	if (is_moving) {
		PushEvent<event::PlayerMoveHeld>(entity, GetDirection());
	}

	if (was_moving && !is_moving) {
		PushEvent<event::PlayerMoveStop>(entity, GetPreviousDirection());
	}

	InvokeMoveCallbacks<
		event::PlayerMoveDirectionStart, event::PlayerMoveDirectionHeld,
		event::PlayerMoveDirectionStop>(entity, WasMoving(Up), IsMoving(Up), Up);

	InvokeMoveCallbacks<
		event::PlayerMoveDirectionStart, event::PlayerMoveDirectionHeld,
		event::PlayerMoveDirectionStop>(entity, WasMoving(Down), IsMoving(Down), Down);

	InvokeMoveCallbacks<
		event::PlayerMoveDirectionStart, event::PlayerMoveDirectionHeld,
		event::PlayerMoveDirectionStop>(entity, WasMoving(Left), IsMoving(Left), Left);

	InvokeMoveCallbacks<
		event::PlayerMoveDirectionStart, event::PlayerMoveDirectionHeld,
		event::PlayerMoveDirectionStop>(entity, WasMoving(Right), IsMoving(Right), Right);
}

bool TopDownMovement::GetMovingState(V2_float d, MoveDirection direction) {
	switch (direction) {
		using enum MoveDirection;
		case None:		return d.x == 0.0f && d.y == 0.0f;
		case Left:		return d.x == -1.0f;
		case Right:		return d.x == 1.0f;
		case Up:		return d.y == -1.0f;
		case Down:		return d.y == 1.0f;
		case UpLeft:	return d.x == -1.0f && d.y == -1.0f;
		case UpRight:	return d.x == 1.0f && d.y == -1.0f;
		case DownLeft:	return d.x == -1.0f && d.y == 1.0f;
		case DownRight: return d.x == 1.0f && d.y == 1.0f;
		default:		PTGN_ERROR("Unknown MoveDirection: ", std::to_underlying(direction));
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

void TopDownMovement::Move(V2_float direction) {
	PTGN_ASSERT(
		!only_orthogonal_movement,
		"Cannot move entity in orthogonal direction unless orthogonal movement is enabled"
	);
	// PTGN_ASSERT(direction.MagnitudeSquared() <= 1.0f, "Move direction must be normalized");
	dir = direction;
}

void TopDownMovement::Move(MoveDirection direction) {
	switch (direction) {
		using enum MoveDirection;
		case None:
			left_input	= false;
			right_input = false;
			down_input	= false;
			up_input	= false;
			break;
		case Left:	left_input = true; break;
		case Right: right_input = true; break;
		case Up:	up_input = true; break;
		case Down:	down_input = true; break;
		case UpLeft:
			up_input   = true;
			left_input = true;
			break;
		case UpRight:
			up_input	= true;
			right_input = true;
			break;
		case DownLeft:
			down_input = true;
			left_input = true;
			break;
		case DownRight:
			down_input	= true;
			right_input = true;
			break;
		default: PTGN_ERROR("Unrecognized movement direction");
	}
}

void TopDownMovement::RunWithAcceleration(V2_float desired_velocity, RigidBody& rb, secondsf dt)
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
				max_speed_change = turn_speed * dt.count();
			} else {
				// If they match, it means we're simply running along and so should use the
				// acceleration stat
				max_speed_change = acceleration * dt.count();
			}
		} else {
			// And if we're not pressing a direction at all, use the deceleration stat
			max_speed_change = deceleration * dt.count();
		}

		// Move our velocity towards the desired velocity, at the rate of the number calculated
		// above
		rb.velocity[i] = impl::MoveTowards(rb.velocity[i], desired_velocity[i], max_speed_change);
	};

	set_velocity(0);
	set_velocity(1);
}

void PlatformerMovement::Update(
	const Scene& scene, Transform& transform, RigidBody& rb, secondsf dt
) const {
	const auto& input{ scene.ctx().input };

	bool left{ input.KeyHeld(left_key) };
	bool right{ input.KeyHeld(right_key) };

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
		transform.SetScaleX(std::abs(transform.GetScale().x) * Sign(dir_x));
	}

	// Calculate's the character's desired velocity - which is the direction you are facing,
	// multiplied by the character's maximum speed
	V2_float desired_velocity{ dir_x * std::max(max_speed - friction, 0.0f), 0.0f };

	// Calculate movement, depending on whether "Instant Movement" has been checked
	if (use_acceleration) {
		RunWithAcceleration(scene, desired_velocity, dir_x, rb, dt);
	} else {
		if (grounded) {
			rb.velocity.x = desired_velocity.x;
		} else {
			RunWithAcceleration(scene, desired_velocity, dir_x, rb, dt);
		}
	}
}

void PlatformerMovement::RunWithAcceleration(
	const Scene& scene, V2_float desired_velocity, float dir_x, RigidBody& rb, secondsf dt
) const {
	const auto& input{ scene.ctx().input };

	// Set our acceleration, deceleration, and turn speed stats, based on whether we're on the
	// ground on in the air

	float acceleration{ grounded ? max_acceleration : max_air_acceleration };
	float deceleration{ grounded ? max_deceleration : max_air_deceleration };
	float turn_speed{ grounded ? max_turn_speed : max_air_turn_speed };

	bool left{ input.KeyHeld(left_key) };
	bool right{ input.KeyHeld(right_key) };
	bool pressing_key{ (left && !right) || (!left && right) };

	float max_speed_change{ 0.0f };

	if (pressing_key) {
		// If the sign (i.e. positive or negative) of our input direction doesn't match our
		// movement, it means we're turning around and so should use the turn speed stat.
		if (!NearlyEqual(Sign(dir_x), Sign(rb.velocity.x))) {
			max_speed_change = turn_speed * dt.count();
		} else {
			// If they match, it means we're simply running along and so should use the
			// acceleration stat
			max_speed_change = acceleration * dt.count();
		}
	} else {
		// And if we're not pressing a direction at all, use the deceleration stat
		max_speed_change = deceleration * dt.count();
	}

	// Move our velocity towards the desired velocity, at the rate of the number calculated
	// above
	rb.velocity.x = impl::MoveTowards(rb.velocity.x, desired_velocity.x, max_speed_change);
}

void PlatformerJump::Ground(Entity entity, const Collision& collision, ColliderMask ground_mask) {
	if (!entity.Has<PlatformerMovement>()) {
		return;
	}

	PTGN_ASSERT((collision.entity.Has<Collider>()));

	if (bool is_ground_collision{ collision.normal == V2_float{ 0.0f, -1.0f } };
		!is_ground_collision) {
		return;
	}

	if (collision.entity.Has<Collider>() && collision.entity.Get<Collider>().IsMask(ground_mask)) {
		entity.Get<PlatformerMovement>().grounded = true;
	}
}

void PlatformerJump::Update(const Scene& scene, RigidBody& rb, bool grounded, V2_float gravity) {
	const auto& input{ scene.ctx().input };

	bool pressed_jump{ input.KeyPressed(jump_key) };

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

	CalculateGravity(scene, rb, grounded, gravity);

	// Situations where pressing jump triggers a jump:
	// 1. On ground.
	// 2. During coyote time.
	// 3. During jump buffer time.

	if ((pressed_jump && grounded) || (grounded && jump_buffered) ||
		(pressed_jump && in_coyote && !grounded)) {
		Jump(rb, gravity);
	}
}

void PlatformerJump::Jump(RigidBody& rb, V2_float gravity) {
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
		jump_speed += std::abs(rb.velocity.y);
	}

	rb.velocity.y -= jump_speed;

	// if (juice != null) {
	//	// Apply the jumping effects on the juice script
	//	juice.jumpEffects();
	// }
}

void PlatformerJump::CalculateGravity(
	const Scene& scene, RigidBody& rb, bool grounded, V2_float gravity
) const {
	const auto& input{ scene.ctx().input };

	float gravity_multiplier{ 0.0f };

	if (grounded) {
		gravity_multiplier = default_gravity_scale;
	} else if (downward_key_speedup && input.KeyHeld(down_key)) {
		gravity_multiplier = downward_speedup_gravity_multiplier;
	} else if (rb.velocity.y < -0.01f) {
		if (!variable_jump_height ||
			(variable_jump_height && input.KeyHeld(jump_key) && jumping_)) {
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
