#pragma once

#include "ecs/ecs.h"
#include "math/vector2.h"

namespace ptgn {

class Scene;

namespace impl {

class Game;

} // namespace impl

class Physics {
public:
	[[nodiscard]] V2_float GetGravity() const;
	void SetGravity(const V2_float& gravity);

	// @return Physics time step in seconds.
	[[nodiscard]] float dt() const;

private:
	friend class impl::Game;
	friend class ptgn::Scene;

	void PreCollisionUpdate(ecs::Manager& manager) const;
	void PostCollisionUpdate(ecs::Manager& manager) const;

	V2_float gravity_{ 0.0f, 0.0f };
};

/**
 * Calculates a Body's per-axis velocity.
 * @param body - The Body to compute the velocity for.
 * @param {number} delta - The delta value to be used in the calculation, in seconds.
 */
/*
computeVelocity : function(body, delta) {
	var velocityX	  = body.velocity.x;
	var accelerationX = body.acceleration.x;
	var dragX		  = body.drag.x;
	var maxX		  = body.maxVelocity.x;

	var velocityY	  = body.velocity.y;
	var accelerationY = body.acceleration.y;
	var dragY		  = body.drag.y;
	var maxY		  = body.maxVelocity.y;

	var speed	   = body.speed;
	var maxSpeed   = body.maxSpeed;
	var allowDrag  = body.allowDrag;
	var useDamping = body.useDamping;

	if (body.allowGravity) {
		velocityX += (this.gravity.x + body.gravity.x) * delta;
		velocityY += (this.gravity.y + body.gravity.y) * delta;
	}

	if (accelerationX) {
		velocityX += accelerationX * delta;
	} else if (allowDrag && dragX) {
		if (useDamping) {
			//  Damping based deceleration
			dragX = Math.pow(dragX, delta);

			velocityX *= dragX;

			speed = Math.sqrt(velocityX * velocityX + velocityY * velocityY);

			if (FuzzyEqual(speed, 0, 0.001)) {
				velocityX = 0;
			}
		} else {
			//  Linear deceleration
			dragX *= delta;

			if (FuzzyGreaterThan(velocityX - dragX, 0, 0.01)) {
				velocityX -= dragX;
			} else if (FuzzyLessThan(velocityX + dragX, 0, 0.01)) {
				velocityX += dragX;
			} else {
				velocityX = 0;
			}
		}
	}

	if (accelerationY) {
		velocityY += accelerationY * delta;
	} else if (allowDrag && dragY) {
		if (useDamping) {
			//  Damping based deceleration
			dragY = Math.pow(dragY, delta);

			velocityY *= dragY;

			speed = Math.sqrt(velocityX * velocityX + velocityY * velocityY);

			if (FuzzyEqual(speed, 0, 0.001)) {
				velocityY = 0;
			}
		} else {
			//  Linear deceleration
			dragY *= delta;

			if (FuzzyGreaterThan(velocityY - dragY, 0, 0.01)) {
				velocityY -= dragY;
			} else if (FuzzyLessThan(velocityY + dragY, 0, 0.01)) {
				velocityY += dragY;
			} else {
				velocityY = 0;
			}
		}
	}

	velocityX = Clamp(velocityX, -maxX, maxX);
	velocityY = Clamp(velocityY, -maxY, maxY);

	body.velocity.set(velocityX, velocityY);

	if (maxSpeed > -1 && body.velocity.length() > maxSpeed) {
		body.velocity.normalize().scale(maxSpeed);
		speed = maxSpeed;
	}

	body.speed = speed;
}
*/

// RigidBody

// velocity = ApplyForces(velocity);
// velocity *= Mathf.Clamp01(1-drag * dt);
// or
// velocity *= 1 / (1 + drag*dt);
// velocity = ApplyCollisionForces(velocity);
// position += velocity * dt;

// if (velocity.MagnitudeSquared() > max_velocity * max_velocity) {
//     velocity = velocity.Normalized() * max_velocity;
// }

} // namespace ptgn