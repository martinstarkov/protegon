#include "physics/collision/collision_handler.h"

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
#include "events/input_handler.h"
#include "math/math.h"
#include "math/vector2.h"
#include "physics/collision/collider.h"
#include "physics/collision/raycast.h"
#include "physics/rigid_body.h"
#include "scene/scene.h"

namespace ptgn::impl {

V2_float CollisionHandler::GetRelativeVelocity(const V2_float& velocity, Entity entity2) {
	V2_float relative_velocity{ velocity };
	if (entity2.Has<RigidBody>()) {
		// TODO: Use physics.dt() here and elsewhere.
		relative_velocity -= entity2.Get<RigidBody>().velocity * game.dt();
	}
	return relative_velocity;
}

void CollisionHandler::AddEarliestCollisions(
	Entity entity, const std::vector<SweepCollision>& sweep_collisions,
	std::unordered_set<Collision>& entities
) {
	PTGN_ASSERT(!sweep_collisions.empty());

	const auto& first_sweep{ sweep_collisions.front() };

	PTGN_ASSERT(entity != first_sweep.entity, "Self collision not possible");

	entities.emplace(first_sweep.entity, first_sweep.collision.normal);

	for (std::size_t i{ 1 }; i < sweep_collisions.size(); ++i) {
		const auto& sweep{ sweep_collisions[i] };

		if (sweep.collision.t == first_sweep.collision.t) {
			PTGN_ASSERT(entity != sweep.entity, "Self collision not possible");
			entities.emplace(sweep.entity, sweep.collision.normal);
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
			if (a.collision.t == b.collision.t) {
				return a.collision.normal.MagnitudeSquared() <
					   b.collision.normal.MagnitudeSquared();
			}
			// If collision times are not equal, sort by collision time.
			return a.collision.t < b.collision.t;
		}
	);
}

V2_float CollisionHandler::GetRemainingVelocity(
	const V2_float& velocity, const RaycastResult& collision, CollisionResponse response
) {
	float remaining_time{ 1.0f - collision.t };

	switch (response) {
		case CollisionResponse::Slide: {
			auto tangent{ -collision.normal.Skewed() };
			return velocity.Dot(tangent) * tangent * remaining_time;
		}
		case CollisionResponse::Push: {
			return Sign(velocity.Dot(-collision.normal.Skewed())) * collision.normal.Swapped() *
				   remaining_time * velocity.Magnitude();
		}
		case CollisionResponse::Bounce: {
			auto new_velocity{ velocity * remaining_time };
			if (!NearlyEqual(Abs(collision.normal.x), 0.0f)) {
				new_velocity.x *= -1.0f;
			}
			if (!NearlyEqual(Abs(collision.normal.y), 0.0f)) {
				new_velocity.y *= -1.0f;
			}
			return new_velocity;
		}
		case CollisionResponse::Stick: {
			return {};
		}
		default: break;
	}
	PTGN_ERROR("Failed to identify DynamicCollisionResponse type");
}

void CollisionHandler::Update(Scene& scene) {
	auto boxes{ scene.EntitiesWith<Enabled, BoxCollider>().GetVector() };
	auto circles{ scene.EntitiesWith<Enabled, CircleCollider>().GetVector() };

	for (const auto& entity1 : boxes) {
		entity1.Get<BoxCollider>().ResetCollisions();
	}
	for (const auto& entity1 : circles) {
		entity1.Get<CircleCollider>().ResetCollisions();
	}

	for (const auto& entity1 : boxes) {
		HandleCollisions<BoxCollider>(entity1, boxes, circles);
	}

	for (const auto& entity1 : circles) {
		HandleCollisions<CircleCollider>(entity1, boxes, circles);
	}

	scene.Refresh();
}

} // namespace ptgn::impl