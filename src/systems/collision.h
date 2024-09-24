#pragma once

#include <ecs/ecs.h>

#include <functional>

#include "components/collider.h"
#include "components/transform.h"
#include "protegon/collision.h"

namespace ptgn {

using CollisionCallback = std::function<void(ecs::Entity, ecs::Entity)>;
using ProcessCallback	= std::function<bool(ecs::Entity, ecs::Entity)>;

static bool CanCollide(const ecs::Entity& body1, const ecs::Entity& body2) {
	if (!body1.IsAlive()) {
		return false;
	}
	if (!body2.IsAlive()) {
		return false;
	}
	// TODO: Add collision masks from component.
	// const auto& collision_mask1		= body1.Get<...>().collision_mask;
	// const auto& collision_category2 = body2.Get<...>().collision_category;
	bool in_category1 = true; // (collision_mask1 & collision_category2) != 0;
	if (!in_category1) {
		return false;
	}
	// TODO: Add collision masks from component
	// const auto& collision_mask2 = body2.Get<...>().collision_mask;
	// const auto& collision_category1 = body1.Get<...>().collision_category;
	bool in_category2 = true; // (collision_mask2 & collision_category1) != 0;
	if (!in_category2) {
		return false;
	}
	return true;
}

static bool Overlap(const ecs::Entity& body1, const ecs::Entity& body2) {
	PTGN_ASSERT(body1.Has<Transform>());
	PTGN_ASSERT(body2.Has<Transform>());
	PTGN_ASSERT((body1.HasAny<BoxCollider, CircleCollider>()));
	PTGN_ASSERT((body2.HasAny<BoxCollider, CircleCollider>()));

	if (body1 == body2) {
		return false;
	}

	const auto& pos1 = body1.Get<Transform>().position;
	const auto& pos2 = body2.Get<Transform>().position;

	if (body1.Has<BoxCollider>()) {
		const auto& box = body1.Get<BoxCollider>();
		Rectangle rect1{ pos1 + box.offset, box.size, box.origin };
		if (body2.Has<BoxCollider>()) {
			const auto& box2 = body2.Get<BoxCollider>();
			Rectangle rect2{ pos2 + box2.offset, box2.size, box2.origin };
			return OverlapCollision::RectangleRectangle(rect1, rect2);
		} else if (body2.Has<CircleCollider>()) {
			const auto& circle2 = body2.Get<CircleCollider>();
			Circle c2{ pos2 + circle2.offset, circle2.radius };
			return OverlapCollision::CircleRectangle(c2, rect1);
		}
	} else if (body1.Has<CircleCollider>()) {
		const auto& circle = body1.Get<CircleCollider>();
		Circle c1{ pos1 + circle.offset, circle.radius };
		if (body2.Has<BoxCollider>()) {
			const auto& box2 = body2.Get<BoxCollider>();
			Rectangle rect2{ pos2 + box2.offset, box2.size, box2.origin };
			return OverlapCollision::CircleRectangle(c1, rect2);
		} else if (body2.Has<CircleCollider>()) {
			const auto& circle2 = body2.Get<CircleCollider>();
			Circle c2{ pos2 + circle2.offset, circle2.radius };
			return OverlapCollision::CircleCircle(c1, c2);
		}
	}

	PTGN_ERROR("Failed to find collision overlap between body1 and body2");
}

struct CircleSeparation {
	float overlap{ 0.0f };
	bool result{ false };
	float x{ 0.0f };
	float y{ 0.0f };
};

static CircleSeparation SeparateCircle(ecs::Entity& body1, ecs::Entity& body2, bool overlap_only) {
	// TODO: Implement this.

	return {};
	//  Set the AABB overlap, blocked and touching values into the bodies (we don't use the return
	//  values here)
	/*
	GetOverlapX(body1, body2, false, 0);
	GetOverlapY(body1, body2, false, 0);

	bool body1IsCircle	   = body1_is_circle;
	bool body2IsCircle	   = body2_is_circle;
	V2_float body1Center   = body1.center;
	V2_float body2Center   = body2.center;
	bool body1Immovable	   = body1.immovable;
	bool body2Immovable	   = body2.immovable;
	V2_float body1Velocity = body1.velocity;
	V2_float body2Velocity = body2.velocity;

	float overlap	= 0;
	bool twoCircles = true;

	if (body1IsCircle != body2IsCircle) {
		twoCircles = false;

		float circleX	   = body1Center.x;
		float circleY	   = body1Center.y;
		float circleRadius = body1.halfWidth;

		float rectX		 = body2.position.x;
		float rectY		 = body2.position.y;
		float rectRight	 = body2.right;
		float rectBottom = body2.bottom;

		if (body2IsCircle) {
			circleX		 = body2Center.x;
			circleY		 = body2Center.y;
			circleRadius = body2.halfWidth;

			rectX	   = body1.position.x;
			rectY	   = body1.position.y;
			rectRight  = body1.right;
			rectBottom = body1.bottom;
		}

		if (circleY < rectY) {
			if (circleX < rectX) {
				overlap = DistanceBetween(circleX, circleY, rectX, rectY) - circleRadius;
			} else if (circleX > rectRight) {
				overlap = DistanceBetween(circleX, circleY, rectRight, rectY) - circleRadius;
			}
		} else if (circleY > rectBottom) {
			if (circleX < rectX) {
				overlap = DistanceBetween(circleX, circleY, rectX, rectBottom) - circleRadius;
			} else if (circleX > rectRight) {
				overlap = DistanceBetween(circleX, circleY, rectRight, rectBottom) - circleRadius;
			}
		}

		//  If a collision occurs in the corner points of the rectangle
		//  then the bodies behave like circles
		overlap *= -1;
	} else {
		overlap =
			(body1.halfWidth + body2.halfWidth) - DistanceBetweenPoints(body1Center, body2Center);
	}

	body1.overlapR = overlap;
	body2.overlapR = overlap;

	float angle	   = AngleBetweenPoints(body1Center, body2Center);
	float overlapX = (overlap + EPSILON) * std::cos(angle);
	float overlapY = (overlap + EPSILON) * std::sin(angle);

	CircleSeparation results{ overlap, false, overlapX, overlapY };

	//  We know the AABBs already intersect before we enter this method
	if (overlap_only && (!twoCircles || (twoCircles && overlap != 0))) {
		//  Now we know the rect vs circle overlaps, or two circles do
		results.result = true;

		return results;
	}

	//  Can't separate (in this method):
	//  Two immovable bodies
	//  A body with its own custom separation logic
	//  A circle vs. a rect with a face-on collision
	if ((!twoCircles && overlap == 0) || (body1Immovable && body2Immovable) ||
		body1.customSeparateX || body2.customSeparateX) {
		//  Let SeparateX / SeparateY handle this
		results.x = undefined;
		results.y = undefined;

		return results;
	}

	//  If we get this far we either have circle vs. circle
	//  or circle vs. rect with corner collision

	bool deadlock = (!body1.pushable && !body2.pushable);

	if (twoCircles) {
		float dx = body1Center.x - body2Center.x;
		float dy = body1Center.y - body2Center.y;
		float d	 = std::sqrt(Math.pow(dx, 2) + Math.pow(dy, 2));
		float nx = ((body2Center.x - body1Center.x) / d) || 0;
		float ny = ((body2Center.y - body1Center.y) / d) || 0;
		float p	 = 2 *
				  (body1Velocity.x * nx + body1Velocity.y * ny - body2Velocity.x * nx -
				   body2Velocity.y * ny) /
				  (body1.mass + body2.mass);

		if (body1Immovable || body2Immovable || !body1.pushable || !body2.pushable) {
			p *= 2;
		}

		if (!body1Immovable && body1.pushable) {
			body1Velocity.x = (body1Velocity.x - p / body1.mass * nx);
			body1Velocity.y = (body1Velocity.y - p / body1.mass * ny);
			body1Velocity.multiply(body1.bounce);
		}

		if (!body2Immovable && body2.pushable) {
			body2Velocity.x = (body2Velocity.x + p / body2.mass * nx);
			body2Velocity.y = (body2Velocity.y + p / body2.mass * ny);
			body2Velocity.multiply(body2.bounce);
		}

		if (!body1Immovable && !body2Immovable) {
			overlapX *= 0.5;
			overlapY *= 0.5;
		}

		if (!body1Immovable || body1.pushable || deadlock) {
			body1.x -= overlapX;
			body1.y -= overlapY;

			body1.updateCenter();
		} else if (!body2Immovable || body2.pushable || deadlock) {
			body2.x += overlapX;
			body2.y += overlapY;

			body2.updateCenter();
		}

		results.result = true;
	} else {
		//  Circle vs. Rect
		//  We'll only move the circle (if we can) and let
		//  the runSeparation handle the rectangle

		if (!body1Immovable || body1.pushable || deadlock) {
			body1.x -= overlapX;
			body1.y -= overlapY;

			body1.updateCenter();
		} else if (!body2Immovable || body2.pushable || deadlock) {
			body2.x += overlapX;
			body2.y += overlapY;

			body2.updateCenter();
		}

		//  Let SeparateX / SeparateY handle this further
		results.x = undefined;
		results.y = undefined;
	}

	return results;
	*/
}

/**
 * @param {number} x - The amount to add to the Body position.
 * @param {number} [vx] - The amount to add to the Body velocity.
 * @param {boolean} [left] - Set the blocked.left value?
 * @param {boolean} [right] - Set the blocked.right value?
 */
void BodyProcessX(ecs::Entity& body, float x, float vx, bool left, bool right) {
	this.x += x;

	this.updateCenter();

	if (vx != null) {
		this.velocity.x = vx;
	}

	auto& blocked = this.blocked;

	if (left) {
		blocked.left = true;
	}

	if (right) {
		blocked.right = true;
	}
}

/**
 * @param {number} y - The amount to add to the Body position.
 * @param {number} [vy] - The amount to add to the Body velocity.
 * @param {boolean} [up] - Set the blocked.up value?
 * @param {boolean} [down] - Set the blocked.down value?
 */
void BodyProcessY(ecs::Entity& body, float y, float vy, bool up, bool down) {
	this.y += y;

	this.updateCenter();

	if (vy != = null) {
		this.velocity.y = vy;
	}

	auto& blocked = this.blocked;

	if (up) {
		blocked.up = true;
	}

	if (down) {
		blocked.down = true;
	}
}

/**
 * Calculates and returns the horizontal overlap between two arcade physics bodies and sets their
 * properties accordingly, including: `touching.left`, `touching.right`, `touching.none` and
 * `overlapX'.
 * @param {Phaser.Physics.Arcade.Body} body1 - The first Body to separate.
 * @param {Phaser.Physics.Arcade.Body} body2 - The second Body to separate.
 * @param {boolean} overlap_only - Is this an overlap only check, or part of separation?
 * @param {number} bias - A value added to the delta values during collision checks. Increase it to
 * prevent sprite tunneling(sprites passing through another instead of colliding).
 * @return {number} The amount of overlap.
 */
float GetOverlapX(ecs::Entity& body1, ecs::Entity& body2, bool overlap_only, float bias) {
	float overlap	 = 0;
	float maxOverlap = body1.deltaAbsX() + body2.deltaAbsX() + bias;

	if (body1._dx == 0 && body2._dx == 0) {
		//  They overlap but neither of them are moving
		body1.embedded = true;
		body2.embedded = true;
	} else if (body1._dx > body2._dx) {
		//  Body1 is moving right and / or Body2 is moving left
		overlap = body1.right - body2.x;

		if ((overlap > maxOverlap && !overlap_only) || body1.checkCollision.right == false ||
			body2.checkCollision.left == false) {
			overlap = 0;
		} else {
			body1.touching.none	 = false;
			body1.touching.right = true;

			body2.touching.none = false;
			body2.touching.left = true;

			if (body2.physicsType == CONST.STATIC_BODY && !overlap_only) {
				body1.blocked.none	= false;
				body1.blocked.right = true;
			}

			if (body1.physicsType == CONST.STATIC_BODY && !overlap_only) {
				body2.blocked.none = false;
				body2.blocked.left = true;
			}
		}
	} else if (body1._dx < body2._dx) {
		//  Body1 is moving left and/or Body2 is moving right
		overlap = body1.x - body2.width - body2.x;

		if ((-overlap > maxOverlap && !overlap_only) || body1.checkCollision.left == false ||
			body2.checkCollision.right == false) {
			overlap = 0;
		} else {
			body1.touching.none = false;
			body1.touching.left = true;

			body2.touching.none	 = false;
			body2.touching.right = true;

			if (body2.physicsType == CONST.STATIC_BODY && !overlap_only) {
				body1.blocked.none = false;
				body1.blocked.left = true;
			}

			if (body1.physicsType == CONST.STATIC_BODY && !overlap_only) {
				body2.blocked.none	= false;
				body2.blocked.right = true;
			}
		}
	}

	//  Resets the overlapX to zero if there is no overlap, or to the actual pixel value if there is
	body1.overlapX = overlap;
	body2.overlapX = overlap;

	return overlap;
}

/**
 * Calculates and returns the vertical overlap between two arcade physics bodies and sets their
 * properties accordingly, including: `touching.up`, `touching.down`, `touching.none` and
 * `overlapY'.
 * @param {Phaser.Physics.Arcade.Body} body1 - The first Body to separate.
 * @param {Phaser.Physics.Arcade.Body} body2 - The second Body to separate.
 * @param {boolean} overlap_only - Is this an overlap only check, or part of separation?
 * @param {number} bias - A value added to the delta values during collision checks. Increase it to
 * prevent sprite tunneling(sprites passing through another instead of colliding).
 * @return {number} The amount of overlap.
 */
float GetOverlapY(ecs::Entity& body1, ecs::Entity& body2, bool overlap_only, float bias) {
	float overlap	 = 0;
	float maxOverlap = body1.deltaAbsY() + body2.deltaAbsY() + bias;

	if (body1._dy == 0 && body2._dy == 0) {
		//  They overlap but neither of them are moving
		body1.embedded = true;
		body2.embedded = true;
	} else if (body1._dy > body2._dy) {
		//  Body1 is moving down and/or Body2 is moving up
		overlap = body1.bottom - body2.y;

		if ((overlap > maxOverlap && !overlap_only) || body1.checkCollision.down == false ||
			body2.checkCollision.up == false) {
			overlap = 0;
		} else {
			body1.touching.none = false;
			body1.touching.down = true;

			body2.touching.none = false;
			body2.touching.up	= true;

			if (body2.physicsType == CONST.STATIC_BODY && !overlap_only) {
				body1.blocked.none = false;
				body1.blocked.down = true;
			}

			if (body1.physicsType == CONST.STATIC_BODY && !overlap_only) {
				body2.blocked.none = false;
				body2.blocked.up   = true;
			}
		}
	} else if (body1._dy < body2._dy) {
		//  Body1 is moving up and/or Body2 is moving down
		overlap = body1.y - body2.bottom;

		if ((-overlap > maxOverlap && !overlap_only) || body1.checkCollision.up == false ||
			body2.checkCollision.down == false) {
			overlap = 0;
		} else {
			body1.touching.none = false;
			body1.touching.up	= true;

			body2.touching.none = false;
			body2.touching.down = true;

			if (body2.physicsType == CONST.STATIC_BODY && !overlap_only) {
				body1.blocked.none = false;
				body1.blocked.up   = true;
			}

			if (body1.physicsType == CONST.STATIC_BODY && !overlap_only) {
				body2.blocked.none = false;
				body2.blocked.down = true;
			}
		}
	}

	//  Resets the overlapY to zero if there is no overlap, or to the actual pixel value if there is
	body1.overlapY = overlap;
	body2.overlapY = overlap;

	return overlap;
}

/**
 * Sets all of the local processing values and calculates the velocity exchanges.
 * Then runs `BlockCheck` and returns the value from it.
 * @param {Phaser.Physics.Arcade.Body} b1 - The first Body to separate.
 * @param {Phaser.Physics.Arcade.Body} b2 - The second Body to separate.
 * @param {number} ov - The overlap value.
 * @return {number} The BlockCheck result. 0 = not blocked. 1 = Body 1 blocked. 2 = Body 2 blocked.
 */
int ProcessYSet(ecs::Entity& body1, ecs::Entity& body2, float ov) {
	body1Pushable	= body1.pushable;
	body1MovingUp	= body1._dy < 0;
	body1MovingDown = body1._dy > 0;
	body1Stationary = body1._dy == 0;
	body1OnTop		= FastAbs(body1.bottom - body2.y) <= FastAbs(body2.bottom - body1.y);
	body1FullImpact = body2.velocity.y - body1.velocity.y * body1.bounce.y;

	body2Pushable	= body2.pushable;
	body2MovingUp	= body2._dy < 0;
	body2MovingDown = body2._dy > 0;
	body2Stationary = body2._dy == 0;
	body2OnTop		= !body1OnTop;
	body2FullImpact = body1.velocity.y - body2.velocity.y * body2.bounce.y;

	//  negative delta = up, positive delta = down (inc. gravity)
	overlap = FastAbs(ov);

	return ProcessYBlockCheck();
};

/**
 * Blocked Direction checks, because it doesn't matter if an object can be pushed
 * or not, blocked is blocked.
 * @return {number} The BlockCheck result. 0 = not blocked. 1 = Body 1 blocked. 2 = Body 2 blocked.
 */
int ProcessYBlockCheck() {
	//  Body1 is moving down and Body2 is blocked from going down any further
	if (body1MovingDown && body1OnTop && body2.blocked.down) {
		BodyProcessY(body1, -overlap, body1FullImpact, false, true);

		return 1;
	}

	//  Body1 is moving up and Body2 is blocked from going up any further
	if (body1MovingUp && body2OnTop && body2.blocked.up) {
		BodyProcessY(body1, overlap, body1FullImpact, true);

		return 1;
	}

	//  Body2 is moving down and Body1 is blocked from going down any further
	if (body2MovingDown && body2OnTop && body1.blocked.down) {
		BodyProcessY(body2, -overlap, body2FullImpact, false, true);

		return 2;
	}

	//  Body2 is moving up and Body1 is blocked from going up any further
	if (body2MovingUp && body1OnTop && body1.blocked.up) {
		BodyProcessY(body2, overlap, body2FullImpact, true);

		return 2;
	}

	return 0;
};

/**
 * The main check function. Runs through one of the four possible tests and returns the results.
 * @return {boolean} `true` if a check passed, otherwise `false`.
 */
bool ProcessYCheck() {
	float nv1 = std::sqrt((body2.velocity.y * body2.velocity.y * body2.mass) / body1.mass) *
				((body2.velocity.y > 0) ? 1 : -1);
	float nv2 = std::sqrt((body1.velocity.y * body1.velocity.y * body1.mass) / body2.mass) *
				((body1.velocity.y > 0) ? 1 : -1);
	float avg = (nv1 + nv2) * 0.5;

	nv1 -= avg;
	nv2 -= avg;

	body1MassImpact = avg + nv1 * body1.bounce.y;
	body2MassImpact = avg + nv2 * body2.bounce.y;

	//  Body1 hits Body2 on the bottom side
	if (body1MovingUp && body2OnTop) {
		return ProcessYRun(0);
	}

	//  Body2 hits Body1 on the bottom side
	if (body2MovingUp && body1OnTop) {
		return ProcessYRun(1);
	}

	//  Body1 hits Body2 on the top side
	if (body1MovingDown && body1OnTop) {
		return ProcessYRun(2);
	}

	//  Body2 hits Body1 on the top side
	if (body2MovingDown && body2OnTop) {
		return ProcessYRun(3);
	}

	return false;
};

/**
 * The main check function. Runs through one of the four possible tests and returns the results.
 * @param {number} side - The side to test. As passed in by the `Check` function.
 * @return {boolean} Always returns `true`.
 */
bool ProcessYRun(int side) {
	if (body1Pushable && body2Pushable) {
		//  Both pushable, or both moving at the same time, so equal rebound
		overlap *= 0.5;

		if (side == 0 || side == 3) {
			//  body1MovingUp && body2OnTop
			//  body2MovingDown && body2OnTop
			BodyProcessY(body1, overlap, body1MassImpact);
			BodyProcessY(body2, -overlap, body2MassImpact);
		} else {
			//  body2MovingUp && body1OnTop
			//  body1MovingDown && body1OnTop
			BodyProcessY(body1, -overlap, body1MassImpact);
			BodyProcessY(body2, overlap, body2MassImpact);
		}
	} else if (body1Pushable && !body2Pushable) {
		//  Body1 pushable, Body2 not

		if (side == 0 || side == 3) {
			//  body1MovingUp && body2OnTop
			//  body2MovingDown && body2OnTop
			BodyProcessY(body1, overlap, body1FullImpact, true);
		} else {
			//  body2MovingUp && body1OnTop
			//  body1MovingDown && body1OnTop
			BodyProcessY(body1, -overlap, body1FullImpact, false, true);
		}
	} else if (!body1Pushable && body2Pushable) {
		//  Body2 pushable, Body1 not

		if (side == 0 || side == 3) {
			//  body1MovingUp && body2OnTop
			//  body2MovingDown && body2OnTop
			BodyProcessY(body2, -overlap, body2FullImpact, false, true);
		} else {
			//  body2MovingUp && body1OnTop
			//  body1MovingDown && body1OnTop
			BodyProcessY(body2, overlap, body2FullImpact, true);
		}
	} else {
		//  Neither body is pushable, so base it on movement

		float halfOverlap = overlap * 0.5;

		if (side == 0) {
			//  body1MovingUp && body2OnTop

			if (body2Stationary) {
				BodyProcessY(body1, overlap, 0, true);
				BodyProcessY(body2, 0, null, false, true);
			} else if (body2MovingDown) {
				BodyProcessY(body1, halfOverlap, 0, true);
				BodyProcessY(body2, -halfOverlap, 0, false, true);
			} else {
				//  Body2 moving same direction as Body1
				BodyProcessY(body1, halfOverlap, body2.velocity.y, true);
				BodyProcessY(body2, -halfOverlap, null, false, true);
			}
		} else if (side == 1) {
			//  body2MovingUp && body1OnTop

			if (body1Stationary) {
				BodyProcessY(body1, 0, null, false, true);
				BodyProcessY(body2, overlap, 0, true);
			} else if (body1MovingDown) {
				BodyProcessY(body1, -halfOverlap, 0, false, true);
				BodyProcessY(body2, halfOverlap, 0, true);
			} else {
				//  Body1 moving same direction as Body2
				BodyProcessY(body1, -halfOverlap, null, false, true);
				BodyProcessY(body2, halfOverlap, body1.velocity.y, true);
			}
		} else if (side == 2) {
			//  body1MovingDown && body1OnTop

			if (body2Stationary) {
				BodyProcessY(body1, -overlap, 0, false, true);
				BodyProcessY(body2, 0, null, true);
			} else if (body2MovingUp) {
				BodyProcessY(body1, -halfOverlap, 0, false, true);
				BodyProcessY(body2, halfOverlap, 0, true);
			} else {
				//  Body2 moving same direction as Body1
				BodyProcessY(body1, -halfOverlap, body2.velocity.y, false, true);
				BodyProcessY(body2, halfOverlap, null, true);
			}
		} else if (side == 3) {
			//  body2MovingDown && body2OnTop

			if (body1Stationary) {
				BodyProcessY(body1, 0, null, true);
				BodyProcessY(body2, -overlap, 0, false, true);
			} else if (body1MovingUp) {
				BodyProcessY(body1, halfOverlap, 0, true);
				BodyProcessY(body2, -halfOverlap, 0, false, true);
			} else {
				//  Body1 moving same direction as Body2
				BodyProcessY(body1, halfOverlap, body2.velocity.y, true);
				BodyProcessY(body2, -halfOverlap, null, false, true);
			}
		}
	}

	return true;
};

/**
 * This function is run when Body1 is Immovable and Body2 is not.
 * @param {number} blockedState - The block state value.
 */
void ProcessYRunImmovableBody1(int blockedState) {
	if (blockedState == 1) {
		//  But Body2 cannot go anywhere either, so we cancel out velocity
		//  Separation happened in the block check
		body2.velocity.y = 0;
	} else if (body1OnTop) {
		BodyProcessY(body2, overlap, body2FullImpact, true);
	} else {
		BodyProcessY(body2, -overlap, body2FullImpact, false, true);
	}

	//  This is special case code that handles things like horizontally moving platforms you can
	//  ride
	if (body1.moves) {
		float body1Distance =
			body1.directControl ? (body1.x - body1.autoFrame.x) : (body1.x - body1.prev.x);

		body2.x	  += body1Distance * body1.friction.x;
		body2._dx  = body2.x - body2.prev.x;
	}
};

/**
 * This function is run when Body2 is Immovable and Body1 is not.
 * @param {number} blockedState - The block state value.
 */
void ProcessYRunImmovableBody2(int blockedState) {
	if (blockedState == 2) {
		//  But Body1 cannot go anywhere either, so we cancel out velocity
		//  Separation happened in the block check
		body1.velocity.y = 0;
	} else if (body2OnTop) {
		BodyProcessY(body1, overlap, body1FullImpact, true);
	} else {
		BodyProcessY(body1, -overlap, body1FullImpact, false, true);
	}

	//  This is special case code that handles things like horizontally moving platforms you can
	//  ride
	if (body2.moves) {
		float body2Distance =
			body2.directControl ? (body2.x - body2.autoFrame.x) : (body2.x - body2.prev.x);

		body1.x	  += body2Distance * body2.friction.x;
		body1._dx  = body1.x - body1.prev.x;
	}
};

/**
 * Sets all of the local processing values and calculates the velocity exchanges.
 * Then runs `BlockCheck` and returns the value from it.
 * @param {Phaser.Physics.Arcade.Body} b1 - The first Body to separate.
 * @param {Phaser.Physics.Arcade.Body} b2 - The second Body to separate.
 * @param {number} ov - The overlap value.
 * @return {number} The BlockCheck result. 0 = not blocked. 1 = Body 1 blocked. 2 = Body 2 blocked.
 */
int ProcessXSet(ecs::Entity& body1, ecs::Entity& body2, float ov) {
	body1Pushable	 = body1.pushable;
	body1MovingLeft	 = body1._dx < 0;
	body1MovingRight = body1._dx > 0;
	body1Stationary	 = body1._dx == 0;
	body1OnLeft		 = FastAbs(body1.right - body2.x) <= FastAbs(body2.right - body1.x);
	body1FullImpact	 = body2.velocity.x - body1.velocity.x * body1.bounce.x;

	body2Pushable	 = body2.pushable;
	body2MovingLeft	 = body2._dx < 0;
	body2MovingRight = body2._dx > 0;
	body2Stationary	 = body2._dx == 0;
	body2OnLeft		 = !body1OnLeft;
	body2FullImpact	 = body1.velocity.x - body2.velocity.x * body2.bounce.x;

	//  negative delta = up, positive delta = down (inc. gravity)
	overlap = FastAbs(ov);

	return ProcessXBlockCheck();
};

/**
 * Blocked Direction checks, because it doesn't matter if an object can be pushed
 * or not, blocked is blocked.
 * @return {number} The BlockCheck result. 0 = not blocked. 1 = Body 1 blocked. 2 = Body 2 blocked.
 */
int ProcessXBlockCheck() {
	//  Body1 is moving right and Body2 is blocked from going right any further
	if (body1MovingRight && body1OnLeft && body2.blocked.right) {
		BodyProcessX(body1, -overlap, body1FullImpact, false, true);

		return 1;
	}

	//  Body1 is moving left and Body2 is blocked from going left any further
	if (body1MovingLeft && body2OnLeft && body2.blocked.left) {
		BodyProcessX(body1, overlap, body1FullImpact, true);

		return 1;
	}

	//  Body2 is moving right and Body1 is blocked from going right any further
	if (body2MovingRight && body2OnLeft && body1.blocked.right) {
		BodyProcessX(body2, -overlap, body2FullImpact, false, true);

		return 2;
	}

	//  Body2 is moving left and Body1 is blocked from going left any further
	if (body2MovingLeft && body1OnLeft && body1.blocked.left) {
		BodyProcessX(body2, overlap, body2FullImpact, true);

		return 2;
	}

	return 0;
};

/**
 * The main check function. Runs through one of the four possible tests and returns the results.
 * @return {boolean} `true` if a check passed, otherwise `false`.
 */
bool ProcessXCheck() {
	float nv1 = std::sqrt((body2.velocity.x * body2.velocity.x * body2.mass) / body1.mass) *
				((body2.velocity.x > 0) ? 1 : -1);
	float nv2 = std::sqrt((body1.velocity.x * body1.velocity.x * body1.mass) / body2.mass) *
				((body1.velocity.x > 0) ? 1 : -1);
	float avg = (nv1 + nv2) * 0.5;

	nv1 -= avg;
	nv2 -= avg;

	body1MassImpact = avg + nv1 * body1.bounce.x;
	body2MassImpact = avg + nv2 * body2.bounce.x;

	//  Body1 hits Body2 on the right hand side
	if (body1MovingLeft && body2OnLeft) {
		return ProcessXRun(0);
	}

	//  Body2 hits Body1 on the right hand side
	if (body2MovingLeft && body1OnLeft) {
		return ProcessXRun(1);
	}

	//  Body1 hits Body2 on the left hand side
	if (body1MovingRight && body1OnLeft) {
		return ProcessXRun(2);
	}

	//  Body2 hits Body1 on the left hand side
	if (body2MovingRight && body2OnLeft) {
		return ProcessXRun(3);
	}

	return false;
};

/**
 * The main check function. Runs through one of the four possible tests and returns the results.
 * @param {number} side - The side to test. As passed in by the `Check` function.
 * @return {boolean} Always returns `true`.
 */
bool ProcessXRun(int side) {
	if (body1Pushable && body2Pushable) {
		//  Both pushable, or both moving at the same time, so equal rebound
		overlap *= 0.5;

		if (side == 0 || side == 3) {
			//  body1MovingLeft && body2OnLeft
			//  body2MovingRight && body2OnLeft
			BodyProcessX(body1, overlap, body1MassImpact);
			BodyProcessX(body2, -overlap, body2MassImpact);
		} else {
			//  body2MovingLeft && body1OnLeft
			//  body1MovingRight && body1OnLeft
			BodyProcessX(body1, -overlap, body1MassImpact);
			BodyProcessX(body2, overlap, body2MassImpact);
		}
	} else if (body1Pushable && !body2Pushable) {
		//  Body1 pushable, Body2 not

		if (side == 0 || side == 3) {
			//  body1MovingLeft && body2OnLeft
			//  body2MovingRight && body2OnLeft
			BodyProcessX(body1, overlap, body1FullImpact, true);
		} else {
			//  body2MovingLeft && body1OnLeft
			//  body1MovingRight && body1OnLeft
			BodyProcessX(body1, -overlap, body1FullImpact, false, true);
		}
	} else if (!body1Pushable && body2Pushable) {
		//  Body2 pushable, Body1 not

		if (side == 0 || side == 3) {
			//  body1MovingLeft && body2OnLeft
			//  body2MovingRight && body2OnLeft
			BodyProcessX(body2, -overlap, body2FullImpact, false, true);
		} else {
			//  body2MovingLeft && body1OnLeft
			//  body1MovingRight && body1OnLeft
			BodyProcessX(body2, overlap, body2FullImpact, true);
		}
	} else {
		//  Neither body is pushable, so base it on movement

		float halfOverlap = overlap * 0.5;

		if (side == 0) {
			//  body1MovingLeft && body2OnLeft

			if (body2Stationary) {
				BodyProcessX(body1, overlap, 0, true);
				BodyProcessX(body2, 0, null, false, true);
			} else if (body2MovingRight) {
				BodyProcessX(body1, halfOverlap, 0, true);
				BodyProcessX(body2, -halfOverlap, 0, false, true);
			} else {
				//  Body2 moving same direction as Body1
				BodyProcessX(body1, halfOverlap, body2.velocity.x, true);
				BodyProcessX(body2, -halfOverlap, null, false, true);
			}
		} else if (side == 1) {
			//  body2MovingLeft && body1OnLeft

			if (body1Stationary) {
				BodyProcessX(body1, 0, null, false, true);
				BodyProcessX(body2, overlap, 0, true);
			} else if (body1MovingRight) {
				BodyProcessX(body1, -halfOverlap, 0, false, true);
				BodyProcessX(body2, halfOverlap, 0, true);
			} else {
				//  Body1 moving same direction as Body2
				BodyProcessX(body1, -halfOverlap, null, false, true);
				BodyProcessX(body2, halfOverlap, body1.velocity.x, true);
			}
		} else if (side == 2) {
			//  body1MovingRight && body1OnLeft

			if (body2Stationary) {
				BodyProcessX(body1, -overlap, 0, false, true);
				BodyProcessX(body2, 0, null, true);
			} else if (body2MovingLeft) {
				BodyProcessX(body1, -halfOverlap, 0, false, true);
				BodyProcessX(body2, halfOverlap, 0, true);
			} else {
				//  Body2 moving same direction as Body1
				BodyProcessX(body1, -halfOverlap, body2.velocity.x, false, true);
				BodyProcessX(body2, halfOverlap, null, true);
			}
		} else if (side == 3) {
			//  body2MovingRight && body2OnLeft

			if (body1Stationary) {
				BodyProcessX(body1, 0, null, true);
				BodyProcessX(body2, -overlap, 0, false, true);
			} else if (body1MovingLeft) {
				BodyProcessX(body1, halfOverlap, 0, true);
				BodyProcessX(body2, -halfOverlap, 0, false, true);
			} else {
				//  Body1 moving same direction as Body2
				BodyProcessX(body1, halfOverlap, body2.velocity.y, true);
				BodyProcessX(body2, -halfOverlap, null, false, true);
			}
		}
	}

	return true;
};

/**
 * This function is run when Body1 is Immovable and Body2 is not.
 * @param {number} blockedState - The block state value.
 */
void ProcessXRunImmovableBody1(int blockedState) {
	if (blockedState == 1) {
		//  But Body2 cannot go anywhere either, so we cancel out velocity
		//  Separation happened in the block check
		body2.velocity.x = 0;
	} else if (body1OnLeft) {
		BodyProcessX(body2, overlap, body2FullImpact, true);
	} else {
		BodyProcessX(body2, -overlap, body2FullImpact, false, true);
	}

	//  This is special case code that handles things like vertically moving platforms you can ride
	if (body1.moves) {
		float body1Distance =
			body1.directControl ? (body1.y - body1.autoFrame.y) : (body1.y - body1.prev.y);

		body2.y	  += body1Distance * body1.friction.y;
		body2._dy  = body2.y - body2.prev.y;
	}
};

/**
 * This function is run when Body2 is Immovable and Body1 is not.
 * @param {number} blockedState - The block state value.
 */
void ProcessXRunImmovableBody2(int blockedState) {
	if (blockedState == 2) {
		//  But Body1 cannot go anywhere either, so we cancel out velocity
		//  Separation happened in the block check
		body1.velocity.x = 0;
	} else if (body2OnLeft) {
		BodyProcessX(body1, overlap, body1FullImpact, true);
	} else {
		BodyProcessX(body1, -overlap, body1FullImpact, false, true);
	}

	//  This is special case code that handles things like vertically moving platforms you can ride
	if (body2.moves) {
		float body2Distance =
			body2.directControl ? (body2.y - body2.autoFrame.y) : (body2.y - body2.prev.y);

		body1.y	  += body2Distance * body2.friction.y;
		body1._dy  = body1.y - body1.prev.y;
	}
};

/**
 * Separates two overlapping bodies on the X-axis (horizontally).
 *
 * Separation involves moving two overlapping bodies so they don't overlap anymore and adjusting
 * their velocities based on their mass. This is a core part of collision detection.
 *
 * The bodies won't be separated if there is no horizontal overlap between them, if they are static,
 * or if either one uses custom logic for its separation.
 *
 * @param body1 - The first Body to separate.
 * @param body2 - The second Body to separate.
 * @param {boolean} overlap_only - If `true`, the bodies will only have their overlap data set and
 * no separation will take place.
 * @param {number} bias - A value to add to the delta value during overlap checking. Used to prevent
 * sprite tunneling.
 * @param {number} [overlap] - If given then this value will be used as the overlap and no check
 * will be run.
 *
 * @return {boolean} `true` if the two bodies overlap vertically, otherwise `false`.
 */
static bool SeparateX(
	ecs::Entity& body1, ecs::Entity& body2, bool overlap_only, float bias, float overlap
) {
	if (overlap == undefined) {
		overlap = GetOverlapX(body1, body2, overlap_only, bias);
	}

	// TODO: Allow objects to be immovable.
	bool body1Immovable = false; // body1.Get<...>().immovable;
	bool body2Immovable = false; // body2.Get<...>().immovable;

	//  Can't separate two immovable bodies, or a body with its own custom separation logic
	if (overlap_only || overlap == 0 || (body1Immovable && body2Immovable) ||
		body1.customSeparateX || body2.customSeparateX) {
		//  return true if there was some overlap, otherwise false
		return (overlap != 0) || (body1.embedded && body2.embedded);
	}

	int blockedState = ProcessXSet(body1, body2, overlap);

	if (!body1Immovable && !body2Immovable) {
		if (blockedState > 0) {
			return true;
		}

		return ProcessXCheck();
	} else if (body1Immovable) {
		ProcessXRunImmovableBody1(blockedState);
	} else if (body2Immovable) {
		ProcessXRunImmovableBody2(blockedState);
	}

	//  If we got this far then there WAS overlap, and separation is complete, so return true
	return true;
}

/**
 * Separates two overlapping bodies on the Y-axis (vertically).
 *
 * Separation involves moving two overlapping bodies so they don't overlap anymore and adjusting
 * their velocities based on their mass. This is a core part of collision detection.
 *
 * The bodies won't be separated if there is no vertical overlap between them, if they are static,
 * or if either one uses custom logic for its separation.
 *
 * @function Phaser.Physics.Arcade.SeparateY
 * @since 3.0.0
 *
 * @param {Phaser.Physics.Arcade.Body} body1 - The first Body to separate.
 * @param {Phaser.Physics.Arcade.Body} body2 - The second Body to separate.
 * @param {boolean} overlap_only - If `true`, the bodies will only have their overlap data set and
 * no separation will take place.
 * @param {number} bias - A value to add to the delta value during overlap checking. Used to prevent
 * sprite tunneling.
 * @param {number} [overlap] - If given then this value will be used as the overlap and no check
 * will be run.
 *
 * @return {boolean} `true` if the two bodies overlap vertically, otherwise `false`.
 */
bool SeparateY(
	ecs::Entity& body1, ecs::Entity& body2, bool overlap_only, float bias, float overlap
) {
	if (overlap == undefined) {
		overlap = GetOverlapY(body1, body2, overlap_only, bias);
	}

	// TODO: Allow objects to be immovable.
	bool body1Immovable = false; // body1.Get<...>().immovable;
	bool body2Immovable = false; // body2.Get<...>().immovable;

	//  Can't separate two immovable bodies, or a body with its own custom separation logic
	if (overlap_only || overlap == 0 || (body1Immovable && body2Immovable) ||
		body1.customSeparateY || body2.customSeparateY) {
		//  return true if there was some overlap, otherwise false
		return (overlap != 0) || (body1.embedded && body2.embedded);
	}

	int blockedState = ProcessYSet(body1, body2, overlap);

	if (!body1Immovable && !body2Immovable) {
		if (blockedState > 0) {
			return true;
		}

		return ProcessYCheck();
	} else if (body1Immovable) {
		ProcessYRunImmovableBody1(blockedState);
	} else if (body2Immovable) {
		ProcessYRunImmovableBody2(blockedState);
	}

	//  If we got this far then there WAS overlap, and separation is complete, so return true
	return true;
};

static bool Separate(
	ecs::Entity& body1, ecs::Entity& body2, const ProcessCallback& process_callback,
	bool overlap_only
) {
	float overlapX{ 0.0f };
	float overlapY{ 0.0f };

	bool result		   = false;
	bool runSeparation = true;

	// TODO: Enable disabling bodies.
	bool body1_enable				= true;	 // body1.Get<...>().enable;
	bool body2_enable				= true;	 // body2.Get<...>().enable;
	bool body1_check_collision_none = false; // body1.Get<...>().check_collision.none;
	bool body2_check_collision_none = false; // body2.Get<...>().check_collision.none;

	if (!body1_enable || !body2_enable || body1_check_collision_none ||
		body2_check_collision_none || !Overlap(body1, body2)) {
		return result;
	}

	//  They overlap. Is there a custom process callback? If it returns true then we can carry on,
	//  otherwise we should abort.
	if (process_callback && !std::invoke(process_callback, body1, body2)) {
		return result;
	}

	// TODO: Add circle check.
	bool body1_is_circle = false; // body1.Get<...>();
	bool body2_is_circle = false; // body2.Get<...>();

	//  Circle vs. Circle, or Circle vs. Rect
	if (body1_is_circle || body2_is_circle) {
		auto circleResults = SeparateCircle(body1, body2, overlap_only);

		if (circleResults.result) {
			//  We got a satisfactory result from the separateCircle method
			result		  = true;
			runSeparation = false;
		} else {
			//  Further processing required
			overlapX	  = circleResults.x;
			overlapY	  = circleResults.y;
			runSeparation = true;
		}
	}

	if (runSeparation) {
		bool resultX = false;
		bool resultY = false;

		// TODO: Move to config.
		constexpr float OVERLAP_BIAS = 4.0f;

		// TODO: Move to config
		// Always separate overlapping Bodies horizontally before vertically.
		// False (default) means Bodies are first separated on the axis of greater gravity, or the
		// vertical axis if neither is greater.
		constexpr bool FORCE_X = false;

		// TODO: Move to config.
		constexpr V2_float GRAVITY{ 0.0f, 0.0f };

		V2_float gravity = GRAVITY;
		bool force_x	 = FORCE_X;
		float bias		 = OVERLAP_BIAS;

		// TODO: Allow for custom gravity for bodies. Total gravity is the sum of this vector and
		// the simulation's gravity.
		V2_float body1_gravity = gravity; // body1.Get<Gravity>();

		//  Do we separate on x first or y first or both?
		if (overlap_only) {
			//  No separation but we need to calculate overlapX, overlapY, etc.
			resultX = SeparateX(body1, body2, overlap_only, bias, overlapX);
			resultY = SeparateY(body1, body2, overlap_only, bias, overlapY);
		}
            else if (force_x || FastAbs(gravity.y + body1_gravity.y) < FastAbs(gravity.x + body1_gravity.x))
            {
			resultX = SeparateX(body1, body2, overlap_only, bias, overlapX);

			//  Are they still intersecting? Let's do the other axis then
			if (Overlap(body1, body2)) {
				resultY = SeparateY(body1, body2, overlap_only, bias, overlapY);
			}
		} else {
			resultY = SeparateY(body1, body2, overlap_only, bias, overlapY);

			//  Are they still intersecting? Let's do the other axis then
			if (Overlap(body1, body2)) {
				resultX = SeparateX(body1, body2, overlap_only, bias, overlapX);
			}
		}

		result = (resultX || resultY);
	}

	if (result) {
		// TODO: Add event emission for collisions.
		/*if (overlap_only) {
			if (body1.on_overlap || body2.on_overlap) {
				EmitEvent(CollisionEvent::Overlap, body1, body2);
			}
		} else if (body1.on_collide || body2.on_collide) {
			EmitEvent(CollisionEvent::Collide, body1, body2);
		}*/
	}

	return result;
}

static bool CollideEntityEntity(
	ecs::Entity& body1, ecs::Entity& body2, const CollisionCallback& collision_callback,
	const ProcessCallback& process_callback, bool overlap_only
) {
	if (!CanCollide(body1, body2)) {
		return false;
	}

	if (Separate(body1, body2, process_callback, overlap_only)) {
		if (collision_callback) {
			std::invoke(collision_callback, body1, body2);
		}

		// TODO: Increment total collision count.
		// total_collisions_++;
	}

	return true;
}

// Collider
// BoxCollider : public Collider
// CircleCollider : public Collider

} // namespace ptgn