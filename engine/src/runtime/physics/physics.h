#pragma once

#include <optional>
#include <ostream>

#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "serialization/serialize.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;
class SceneContext;

enum class BoundaryBehavior {
	StopVelocity,	// Clamp position and stop velocity.
	SlideVelocity,	// Clamp position and do not change velocity.
	ReflectVelocity // Bounce off bounds by flipping velocity
};

std::ostream& operator<<(std::ostream& os, BoundaryBehavior behavior);

PTGN_REFLECT_ENUM(
	BoundaryBehavior, { { BoundaryBehavior::StopVelocity, "stop_velocity" },
						{ BoundaryBehavior::SlideVelocity, "slide_velocity" },
						{ BoundaryBehavior::ReflectVelocity, "reflect_velocity" } }
);

struct Bounds {
	/// @brief Center position of the bounding box.
	V2_float position;

	V2_float size;

	BoundaryBehavior behavior{ BoundaryBehavior::SlideVelocity };

	friend std::ostream& operator<<(std::ostream& os, const Bounds& bounds) {
		os << "{ position: " << bounds.position;
		os << ", size: " << bounds.size;
		os << ", behavior: " << bounds.behavior << " }";
		return os;
	}
};

class Physics {
public:
	std::optional<Bounds> GetBounds() const;
	/// @param bounds Nullopt results in no boundary enforcement.
	void SetBounds(std::optional<Bounds> bounds = {});

	V2_float GetGravity() const;
	void SetGravity(V2_float gravity);

	/// @return Physics time step. Currently the same as the delta time of the current frame.
	[[nodiscard]] secondsf dt() const;

	void SetEnabled(bool enabled = true);
	void Disable();
	void Enable();

	/// @return True if physics is enabled, false otherwise.
	[[nodiscard]] bool IsEnabled() const;

	// TODO: Fix serialization.
	// PTGN_REFLECT(
	//	Physics, KeyValue("gravity", gravity_), KeyValue("bounds_top_left", bounds_top_left_),
	//	KeyValue("bounds_size", bounds_size_), KeyValue("boundary_behavior", boundary_behavior_),
	//	KeyValue("enabled", enabled_)
	//)

private:
	friend class Scene;
	friend class SceneContext;

	explicit Physics(Scene& scene);

	void PreCollisionUpdate() const;
	void PostCollisionUpdate() const;

	static void HandleBoundary(Transform& transform, V2_float& velocity, const Bounds& bounds);

	Scene& scene_;

	void Reset();

	bool enabled_{ true };
	std::optional<Bounds> bounds_;
	V2_float gravity_{ 0.0f, 0.0f };
};

} // namespace ptgn

/// Calculates a Body's per-axis velocity.
/// @param body - The Body to compute the velocity for.
/// @param {number} delta - The delta value to be used in the calculation, in seconds.
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

	if (maxSpeed.has_value() && body.velocity.length() > maxSpeed) {
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