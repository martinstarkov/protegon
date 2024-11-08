#pragma once

#include <algorithm>
#include <cmath>
#include <utility>

#include "components/transform.h"
#include "core/game.h"
#include "event/input_handler.h"
#include "event/key.h"
#include "math/math.h"
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
	int directionX{ 1 };

	V2_float desiredVelocity;
	float maxSpeedChange;
	float acceleration;
	float deceleration;
	float turnSpeed;

	// TODO: Move to PlatformerJump class.
	bool onGround{ false };

	// Parameters:
	// Maximum movement speed
	float maxSpeed{ 10.0f * 60.0f };
	// How fast to reach max speed
	float maxAcceleration{ 52.0f * 60.0f };
	// How fast to stop after letting go
	float maxDecceleration{ 52.0f * 60.0f };
	// How fast to stop when changing direction
	float maxTurnSpeed{ 80.0f * 60.0f };
	// How fast to reach max speed when in mid-air
	float maxAirAcceleration{ 52.0f * 60.0f };
	// How fast to stop in mid-air when no direction is used
	float maxAirDeceleration{ 52.0f * 60.0f };
	// How fast to stop when changing direction when in mid-air.
	float maxAirTurnSpeed{ 80.0f * 60.0f };

	// Optional parameters:
	bool useAcceleration{ true };
	float friction{ 0.0f };

	Key left_key{ Key::A };
	Key right_key{ Key::D };

	void Update(Transform& transform, RigidBody& rb) {
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
			runWithAcceleration(rb);
		} else {
			if (onGround) {
				runWithoutAcceleration(rb);
			} else {
				runWithAcceleration(rb);
			}
		}
	}

	static float MoveTowards(float current, float target, float maxDelta) {
		if (FastAbs(target - current) <= maxDelta) {
			return target;
		}
		return current + Sign(target - current) * maxDelta;
	}

	void runWithAcceleration(RigidBody& rb) {
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

	void runWithoutAcceleration(RigidBody& rb) {
		// If we're not using acceleration and deceleration, just send our desired velocity
		// (direction * max speed) to the Rigidbody
		rb.velocity.x = desiredVelocity.x;
	}
};

struct PlatformerJump {
public:
	Key jump_key{ Key::W };
	milliseconds jump_buffer_time{ 250 };
	milliseconds coyote_time{ 150 };

private:
	Timer jump_buffer_;
	Timer coyote_timer_;

	float gravMultiplier{ 0.0f };
	float defaultGravityScale{ 1.0f };
	float upwardMovementMultiplier{ 1.0f };
	float downwardMovementMultiplier{ 1.0f };
	bool variablejumpHeight{ true };
	float jumpCutOff{ 1.0f };
	float speedLimit{ 60.0f * 600.0f };
	float jumpHeight{ 360.0f };
	float timeToJumpApex{ 5.0f };
	bool jumping = false;

public:
	void Jump(RigidBody& rb) {
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

	void Update(RigidBody& rb, bool onGround) {
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

	void CalculateGravity(RigidBody& rb, bool onGround) {
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
};

/*
struct PlatformerJump {
	// Maximum jump height
	float jumpHeight{ 7.3f * 60.0f };

	// If you're using your stats from Platformer Toolkit with this character controller, please
	// note that the number on the Jump Duration handle does not match this stat It is re-scaled,
	// from 0.2f - 1.25f, to 1 - 10. You can transform the number on screen to the stat here, using
	// the function at the bottom of this script.

	// How long it takes to reach that height before
	// coming back down
	float timeToJumpApex{ 0.7f };
	// Gravity multiplier to apply when going up
	float upwardMovementMultiplier{ 1.0f };
	// Gravity multiplier to apply when coming down
	float downwardMovementMultiplier{ 6.17f };
	// How many times can you jump in the air?
	int maxAirJumps{ 0 };

	// Options
	// Should the character drop when you let go of jump?
	bool variablejumpHeight{ true };
	// Gravity multiplier when you let go of jump
	float jumpCutOff{ 2.0f };
	// The fastest speed the character can fall
	float speedLimit{ 30.0f * 60.0f };
	// How long should coyote time last?
	float coyoteTime{ 0.15f };
	// How far from ground should we cache your jump?
	float jumpBuffer{ 0.15f };

	// Internal
	float jumpSpeed{ 0.0f };
	float defaultGravityScale{ 1.0f };
	float gravMultiplier{ 1.0f };

	// State
	bool canJumpAgain{ false };
	bool desiredJump{ false };
	float jumpBufferCounter{ 0.0f };
	float coyoteTimeCounter{ 0.0f };
	bool pressingJump{ false };
	bool currentlyJumping{ false };

	void Update(RigidBody& rb, bool onGround) {
		// Keep trying to do a jump, for as long as desiredJump is true
		if (game.input.KeyPressed(Key::SPACE) || game.input.KeyPressed(Key::W)) {
			desiredJump	 = true;
			pressingJump = true;
		} else {
			pressingJump = false;
		}

		if (desiredJump) {
			DoAJump(rb, onGround);

			// TODO: Consider:
			// Skip gravity calculations this frame, so currentlyJumping doesn't turn off
			// This makes sure you can't do the coyote time double jump bug
			return;
		}

		calculateGravity(rb, onGround);

		setPhysics(rb);

		// Jump buffer allows us to queue up a jump, which will play when we next hit the ground

		float dt = game.physics.dt();
		if (jumpBuffer > 0) {
			// Instead of immediately turning off "desireJump", start counting up...
			// All the while, the DoAJump function will repeatedly be fired off
			if (desiredJump) {
				jumpBufferCounter += dt;

				if (jumpBufferCounter > jumpBuffer) {
					// If time exceeds the jump buffer, turn off "desireJump"
					desiredJump		  = false;
					jumpBufferCounter = 0;
				}
			}
		}

		// If we're not on the ground and we're not currently jumping, that means we've stepped off
		// the edge of a platform. So, start the coyote time counter...
		if (!currentlyJumping && !onGround) {
			coyoteTimeCounter += dt;
		} else {
			// Reset it when we touch the ground, or jump
			coyoteTimeCounter = 0;
		}
	}

	void setPhysics(RigidBody& rb) {
		// Determine the character's gravity scale, using the stats provided. Multiply it by a
		// gravMultiplier, used later
		float new_gravity_y = (2.0f * jumpHeight) / (timeToJumpApex * timeToJumpApex);
		rb.gravity			= (new_gravity_y / game.physics.GetGravity().y) * gravMultiplier;
	}

	void calculateGravity(RigidBody& rb, bool onGround) {
		// We change the character's gravity based on her Y direction

		// If Kit is going up...
		if (rb.velocity.y > 0.01f) {
			if (onGround) {
				// Don't change it if Kit is stood on something (such as a moving platform)
				gravMultiplier = defaultGravityScale;
			} else {
				// If we're using variable jump height...)
				if (variablejumpHeight) {
					// Apply upward multiplier if player is rising and holding jump
					if (pressingJump && currentlyJumping) {
						gravMultiplier = upwardMovementMultiplier;
					}
					// But apply a special downward multiplier if the player lets go of jump
					else {
						gravMultiplier = jumpCutOff;
					}
				} else {
					gravMultiplier = upwardMovementMultiplier;
				}
			}
		}

		// Else if going down...
		else if (rb.velocity.y < -0.01f) {
			if (onGround)
			// Don't change it if Kit is stood on something (such as a moving platform)
			{
				gravMultiplier = defaultGravityScale;
			} else {
				// Otherwise, apply the downward gravity multiplier as Kit comes back to Earth
				gravMultiplier = downwardMovementMultiplier;
			}

		}
		// Else not moving vertically at all
		else {
			if (onGround) {
				currentlyJumping = false;
			}

			gravMultiplier = defaultGravityScale;
		}

		// Set the character's Rigidbody's velocity
		// But clamp the Y variable within the bounds of the speed limit, for the terminal velocity
		// assist option
		// rb.velocity.y = std::clamp(rb.velocity.y, -speedLimit, 100.0f * 60.0f);
	}

	void DoAJump(RigidBody& rb, bool onGround) {
		// Create the jump, provided we are on the ground, in coyote time, or have a double jump
		// available
		if (onGround ||
			canJumpAgain (coyoteTimeCounter > 0.03f && coyoteTimeCounter < coyoteTime) ||
		) {
			desiredJump		  = false;
			jumpBufferCounter = 0;
			coyoteTimeCounter = 0;

			// If we have double jump on, allow us to jump again (but only once)
			canJumpAgain = (maxAirJumps == 1 && canJumpAgain == false);

			// Determine the power of the jump, based on our gravity and stats
			jumpSpeed = std::sqrt(2.0f * game.physics.GetGravity().y * rb.gravity * jumpHeight);

			// If Kit is moving up or down when she jumps (such as when doing a double jump), change
			// the jumpSpeed; This will ensure the jump is the exact same strength, no matter your
			// velocity.
			if (rb.velocity.y > 0.0f) {
				jumpSpeed += FastAbs(rb.velocity.y);
			} else if (rb.velocity.y < 0.0f) {
				jumpSpeed = std::max(jumpSpeed - rb.velocity.y, 0.0f);
			}

			// Apply the new jumpSpeed to the velocity. It will be sent to the Rigidbody in
			// FixedUpdate;
			rb.velocity.y	 += -jumpSpeed; // TODO: Consider adding dt multiplier here.
			currentlyJumping  = true;

			// TODO: Apply the jumping effects on the juice script
			// juice.jumpEffects();
		}
		// TODO: Remove in favor of jump buffering.
		// desiredJump = false;

		if (jumpBuffer == 0) {
			// If we don't have a jump buffer, then turn off desiredJump immediately after hitting
			// jumping
			desiredJump = false;
		}
	}

	void bounceUp(RigidBody& rb, float bounceAmount) {
		// Used by the springy pad
		rb.AddImpulse(V2_float::Up() * bounceAmount);
	}
};
*/

} // namespace ptgn