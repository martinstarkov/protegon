#include "physics/movement.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "math/collider.h"
#include "components/transform.h"
#include "core/game.h"
#include "ecs/ecs.h"
#include "event/input_handler.h"
#include "event/key.h"
#include "math/math.h"
#include "math/vector2.h"
#include "physics/physics.h"
#include "rigid_body.h"
#include "utility/debug.h"
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

} // namespace impl

void MoveWASD(V2_float& vel, const V2_float& amount, bool cancel_velocity_if_unpressed) {
	impl::MoveImpl(vel, amount, Key::A, Key::D, Key::W, Key::S, cancel_velocity_if_unpressed);
}

void MoveArrowKeys(V2_float& vel, const V2_float& amount, bool cancel_velocity_if_unpressed) {
	impl::MoveImpl(
		vel, amount, Key::LEFT, Key::RIGHT, Key::UP, Key::DOWN, cancel_velocity_if_unpressed
	);
}

void PlatformerMovement::Update(Transform& transform, RigidBody& rb) {
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
		RunWithAcceleration(desired_velocity, dir_x, rb);
	} else {
		if (grounded) {
			rb.velocity.x = desired_velocity.x;
		} else {
			RunWithAcceleration(desired_velocity, dir_x, rb);
		}
	}
}

float PlatformerMovement::MoveTowards(float current, float target, float maxDelta) {
	if (FastAbs(target - current) <= maxDelta) {
		return target;
	}
	return current + Sign(target - current) * maxDelta;
}

void PlatformerMovement::RunWithAcceleration(
	const V2_float& desired_velocity, float dir_x, RigidBody& rb
) const {
	// Set our acceleration, deceleration, and turn speed stats, based on whether we're on the
	// ground on in the air

	float acceleration{ grounded ? max_acceleration : max_air_acceleration };
	float deceleration{ grounded ? max_deceleration : max_air_deceleration };
	float turn_speed{ grounded ? max_turn_speed : max_air_turn_speed };

	bool left{ game.input.KeyPressed(left_key) };
	bool right{ game.input.KeyPressed(right_key) };
	bool pressing_key{ (left && !right) || (!left && right) };

	float dt{ game.physics.dt() };

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
	rb.velocity.x = MoveTowards(rb.velocity.x, desired_velocity.x, max_speed_change);
}

void PlatformerJump::Ground(Collision c, CollisionCategory ground_category) {
	PTGN_ASSERT((c.entity2.HasAny<BoxCollider, CircleCollider>()));
	if ((c.entity2.Has<BoxCollider>() && c.entity2.Get<BoxCollider>().IsCategory(ground_category)) ||
		(c.entity2.Has<CircleCollider>() &&
			c.entity2.Get<CircleCollider>().IsCategory(ground_category))) {
		if (c.entity1.Has<PlatformerMovement>() && c.normal == V2_float{ 0.0f, -1.0f }) {
			c.entity1.Get<PlatformerMovement>().grounded = true;
		}
	}
}

void PlatformerJump::Update(RigidBody& rb, bool grounded) {
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

	CalculateGravity(rb, grounded);

	// Situations where pressing jump triggers a jump:
	// 1. On ground.
	// 2. During coyote time.
	// 3. During jump buffer time.

	if ((pressed_jump && grounded) || (grounded && jump_buffered) ||
		(pressed_jump && in_coyote && !grounded)) {
		Jump(rb);
	}
}

void PlatformerJump::Jump(RigidBody& rb) {
	jumping_ = true;

	jump_buffer_.Stop();
	coyote_timer_.Stop();

	// If we have double jump on, allow us to jump again (but only once)
	// canJumpAgain = (maxAirJumps == 1 && canJumpAgain == false);

	// Determine the power of the jump, based on our gravity and stats
	float jump_speed{ std::sqrt(2.0f * game.physics.GetGravity().y * rb.gravity * jump_height) };

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

void PlatformerJump::CalculateGravity(RigidBody& rb, bool grounded) const {
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
	rb.gravity = gravity_multiplier * 2 * jump_height /
				 (time_to_jump_apex * time_to_jump_apex * game.physics.GetGravity().y);
}

} // namespace ptgn
