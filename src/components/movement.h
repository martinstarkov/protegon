#pragma once

#include "components/rigid_body.h"
#include "components/transform.h"
#include "core/game.h"
#include "event/input_handler.h"
#include "math/math.h"
#include "math/vector2.h"
#include "physics/physics.h"

namespace ptgn {

struct PlatformerMovement {
	int directionX{ 1 };

	V2_float desiredVelocity;
	float maxSpeedChange;
	float acceleration;
	float deceleration;
	float turnSpeed;

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

	void Update(Transform& transform, RigidBody& rb) {
		bool left{ game.input.KeyPressed(Key::A) };
		bool right{ game.input.KeyPressed(Key::D) };
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

		bool left{ game.input.KeyPressed(Key::A) };
		bool right{ game.input.KeyPressed(Key::D) };
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

} // namespace ptgn