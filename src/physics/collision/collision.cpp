#include "physics/collision/collision.h"

#include <algorithm>
#include <unordered_set>
#include <utility>
#include <vector>

#include "common/assert.h"
#include "components/common.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "debug/log.h"
#include "math/math.h"
#include "math/vector2.h"
#include "physics/collision/collider.h"
#include "physics/collision/raycast.h"
#include "physics/rigid_body.h"

namespace ptgn::impl {

V2_float CollisionHandler::GetRelativeVelocity(const V2_float& vel, Entity e2) {
	V2_float relative_velocity{ vel };
	if (e2.Has<RigidBody>()) {
		// TODO: Use physics.dt() here and elsewhere.
		relative_velocity -= e2.Get<RigidBody>().velocity * game.dt();
	}
	return relative_velocity;
}

void CollisionHandler::AddEarliestCollisions(
	Entity e, const std::vector<SweepCollision>& sweep_collisions,
	std::unordered_set<Collision>& entities
) {
	PTGN_ASSERT(!sweep_collisions.empty());

	const auto& first_collision{ sweep_collisions.front() };

	PTGN_ASSERT(e != first_collision.e, "Self collision not possible");

	entities.emplace(e, first_collision.e, first_collision.c.normal);

	for (std::size_t i{ 1 }; i < sweep_collisions.size(); ++i) {
		const auto& collision{ sweep_collisions[i] };

		if (collision.c.t == first_collision.c.t) {
			PTGN_ASSERT(e != collision.e, "Self collision not possible");
			entities.emplace(e, collision.e, collision.c.normal);
		}
	}
};

void CollisionHandler::SortCollisions(std::vector<SweepCollision>& collisions) {
	/*
	 * Initial sort based on distances of collision manifolds to the collider.
	 * This is required for RectVsRect collisions to prevent sticking
	 * to corners in certain configurations, such as if the player (o) gives
	 * a bottom right velocity into the following rectangle (x) configuration:
	 *       x
	 *     o x
	 *   x   x
	 * (player would stay still instead of moving down if this distance sort did not exist).
	 */
	std::sort(
		collisions.begin(), collisions.end(),
		[](const SweepCollision& a, const SweepCollision& b) { return a.dist2 < b.dist2; }
	);
	// Sort based on collision times, and if they are equal, by collision normal magnitudes.
	std::sort(
		collisions.begin(), collisions.end(),
		[](const SweepCollision& a, const SweepCollision& b) {
			// If time of collision are equal, prioritize walls to corners, i.e. normals
			// (1,0) come before (1,1).
			if (a.c.t == b.c.t) {
				return a.c.normal.MagnitudeSquared() < b.c.normal.MagnitudeSquared();
			}
			// If collision times are not equal, sort by collision time.
			return a.c.t < b.c.t;
		}
	);
}

V2_float CollisionHandler::GetRemainingVelocity(
	const V2_float& velocity, const RaycastResult& c, CollisionResponse response
) {
	float remaining_time{ 1.0f - c.t };

	switch (response) {
		case CollisionResponse::Slide: {
			auto tangent{ -c.normal.Skewed() };
			return velocity.Dot(tangent) * tangent * remaining_time;
		}
		case CollisionResponse::Push: {
			return Sign(velocity.Dot(-c.normal.Skewed())) * c.normal.Swapped() * remaining_time *
				   velocity.Magnitude();
		}
		case CollisionResponse::Bounce: {
			auto new_velocity{ velocity * remaining_time };
			if (!NearlyEqual(Abs(c.normal.x), 0.0f)) {
				new_velocity.x *= -1.0f;
			}
			if (!NearlyEqual(Abs(c.normal.y), 0.0f)) {
				new_velocity.y *= -1.0f;
			}
			return new_velocity;
		}
		default: break;
	}
	PTGN_ERROR("Failed to identify DynamicCollisionResponse type");
}

void CollisionHandler::Update(Manager& manager) {
	auto boxes{ std::as_const(manager).EntitiesWith<Enabled, BoxCollider>() };
	auto circles{ std::as_const(manager).EntitiesWith<Enabled, CircleCollider>() };

	for (auto [entity1, enabled, box1] : boxes) {
		if (!enabled) {
			continue;
		}
		HandleCollisions<BoxCollider>(entity1, boxes, circles);
	}

	for (auto [entity1, enabled, circle1] : circles) {
		if (!enabled) {
			continue;
		}
		HandleCollisions<CircleCollider>(entity1, boxes, circles);
	}

	manager.Refresh();
}

} // namespace ptgn::impl