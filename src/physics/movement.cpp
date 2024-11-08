#include "physics/movement.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "components/transform.h"
#include "core/game.h"
#include "event/input_handler.h"
#include "event/key.h"
#include "math/math.h"
#include "math/vector2.h"
#include "physics/physics.h"
#include "rigid_body.h"
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
	directionX = 0;
	if (left && !right) {
		directionX = -1;
	}
	if (right && !left) {
		directionX = 1;
	}

	// Used to flip the character's sprite when she changes direction
	// Also tells us that we are currently pressing a direction button
	if (directionX != 0) {
		transform.scale.x = FastAbs(transform.scale.x) * (float)Sign(directionX);
	}

	// Calculate's the character's desired velocity - which is the direction you are facing,
	// multiplied by the character's maximum speed
	desiredVelocity = V2_float{ (float)directionX * std::max(maxSpeed - friction, 0.0f), 0.0f };

	// Calculate movement, depending on whether "Instant Movement" has been checked
	if (useAcceleration) {
		RunWithAcceleration(rb);
	} else {
		if (onGround) {
			RunWithoutAcceleration(rb);
		} else {
			RunWithAcceleration(rb);
		}
	}
}

float PlatformerMovement::MoveTowards(float current, float target, float maxDelta) {
	if (FastAbs(target - current) <= maxDelta) {
		return target;
	}
	return current + Sign(target - current) * maxDelta;
}

void PlatformerMovement::RunWithAcceleration(RigidBody& rb) {
	// Set our acceleration, deceleration, and turn speed stats, based on whether we're on the
	// ground on in the air

	acceleration = onGround ? maxAcceleration : maxAirAcceleration;
	deceleration = onGround ? maxDecceleration : maxAirDeceleration;
	turnSpeed	 = onGround ? maxTurnSpeed : maxAirTurnSpeed;

	bool left{ game.input.KeyPressed(left_key) };
	bool right{ game.input.KeyPressed(right_key) };
	bool pressingKey = left && !right || !left && right;

	float dt{ game.physics.dt() };

	if (pressingKey) {
		// If the sign (i.e. positive or negative) of our input direction doesn't match our
		// movement, it means we're turning around and so should use the turn speed stat.
		if (Sign(directionX) != (int)Sign(rb.velocity.x)) {
			maxSpeedChange = turnSpeed * dt;
		} else {
			// If they match, it means we're simply running along and so should use the
			// acceleration stat
			maxSpeedChange = acceleration * dt;
		}
	} else {
		// And if we're not pressing a direction at all, use the deceleration stat
		maxSpeedChange = deceleration * dt;
	}

	// Move our velocity towards the desired velocity, at the rate of the number calculated
	// above
	rb.velocity.x = MoveTowards(rb.velocity.x, desiredVelocity.x, maxSpeedChange);
}

void PlatformerMovement::RunWithoutAcceleration(RigidBody& rb) const {
	// If we're not using acceleration and deceleration, just send our desired velocity
	// (direction * max speed) to the Rigidbody
	rb.velocity.x = desiredVelocity.x;
}

void PlatformerJump::Jump(RigidBody& rb) {
	jumping = true;

	jump_buffer_.Stop();
	coyote_timer_.Stop();

	// If we have double jump on, allow us to jump again (but only once)
	// canJumpAgain = (maxAirJumps == 1 && canJumpAgain == false);

	// Determine the power of the jump, based on our gravity and stats
	float jumpSpeed = std::sqrt(2.0f * game.physics.GetGravity().y * rb.gravity * jumpHeight);

	// If Kit is moving up or down when she jumps (such as when doing a double jump), change
	// the jumpSpeed; This will ensure the jump is the exact same strength, no matter your
	// velocity.
	if (rb.velocity.y < 0.0f) {
		jumpSpeed = std::max(jumpSpeed - rb.velocity.y, 0.0f);
	} else if (rb.velocity.y > 0.0f) {
		jumpSpeed += FastAbs(rb.velocity.y);
	}

	// Apply the new jumpSpeed to the velocity. It will be sent to the Rigidbody in
	// FixedUpdate;
	rb.velocity.y -= jumpSpeed;

	// if (juice != null) {
	//	// Apply the jumping effects on the juice script
	//	juice.jumpEffects();
	// }
}

void PlatformerJump::Update(RigidBody& rb, bool onGround) {
	bool pressed_jump = game.input.KeyDown(jump_key);

	if (onGround) {
		coyote_timer_.Start();
		jumping = false;
	}

	if (pressed_jump && !onGround) {
		// Player desires to jump but currently cant.
		jump_buffer_.Start();
	}

	bool jump_buffered = jump_buffer_.IsRunning() && !jump_buffer_.Completed(jump_buffer_time);
	bool in_coyote	   = coyote_timer_.IsRunning() && !coyote_timer_.Completed(coyote_time);

	// Situations where pressing jump triggers a jump:
	// 1. On ground.
	// 2. During coyote time.
	// 3. During jump buffer time.
	if (pressed_jump && onGround || onGround && jump_buffered ||
		pressed_jump && in_coyote && !onGround) {
		Jump(rb);
	}
}

void PlatformerJump::CalculateGravity(RigidBody& rb, bool onGround) {
	if (onGround || rb.velocity.y > -0.01f && rb.velocity.y < 0.01f) {
		gravMultiplier = defaultGravityScale;
	} else if (rb.velocity.y < -0.01f) {
		if (!variablejumpHeight ||
			variablejumpHeight && game.input.KeyPressed(jump_key) && jumping) {
			gravMultiplier = upwardMovementMultiplier;
		} else if (variablejumpHeight) {
			gravMultiplier = jumpCutOff;
		}
	} else if (rb.velocity.y > 0.01f) {
		gravMultiplier = downwardMovementMultiplier;
	}

	// Set the character's Rigidbody's velocity
	// But clamp the Y variable within the bounds of the speed limit, for the terminal velocity
	// assist option
	rb.velocity.y = std::clamp(rb.velocity.y, -speedLimit, speedLimit);
	// Determine the character's gravity scale, using the stats provided. Multiply it by a
	// gravMultiplier, used later
	rb.gravity =
		((2 * jumpHeight) / (timeToJumpApex * timeToJumpApex) / game.physics.GetGravity().y) *
		gravMultiplier;
}

} // namespace ptgn
