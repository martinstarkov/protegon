#include "math/collision.h"

#include <algorithm>
#include <unordered_set>
#include <vector>

#include "core/game.h"
#include "ecs/ecs.h"
#include "math/collider.h"
#include "math/geometry/line.h"
#include "math/math.h"
#include "math/raycast.h"
#include "math/vector2.h"
#include "physics/rigid_body.h"
#include "renderer/color.h"
#include "scene/scene_manager.h"
#include "utility/assert.h"
#include "utility/log.h"

namespace ptgn::impl {

V2_float CollisionHandler::GetRelativeVelocity(const V2_float& vel, ecs::Entity e2) {
	V2_float relative_velocity{ vel };
	if (e2.Has<RigidBody>()) {
		// TODO: Use physics.dt() here and elsewhere.
		relative_velocity -= e2.Get<RigidBody>().velocity * game.dt();
	}
	return relative_velocity;
}

void CollisionHandler::AddEarliestCollisions(
	ecs::Entity e, const std::vector<SweepCollision>& sweep_collisions,
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
	const V2_float& velocity, const Raycast& c, CollisionResponse response
) {
	float remaining_time{ 1.0f - c.t };

	switch (response) {
		case CollisionResponse::Slide: {
			V2_float tangent{ -c.normal.Skewed() };
			return velocity.Dot(tangent) * tangent * remaining_time;
		}
		case CollisionResponse::Push: {
			return Sign(velocity.Dot(-c.normal.Skewed())) * c.normal.Swapped() * remaining_time *
				   velocity.Magnitude();
		}
		case CollisionResponse::Bounce: {
			V2_float new_velocity = velocity * remaining_time;
			if (!NearlyEqual(FastAbs(c.normal.x), 0.0f)) {
				new_velocity.x *= -1.0f;
			}
			if (!NearlyEqual(FastAbs(c.normal.y), 0.0f)) {
				new_velocity.y *= -1.0f;
			}
			return new_velocity;
		}
		default: break;
	}
	PTGN_ERROR("Failed to identify DynamicCollisionResponse type");
}

void CollisionHandler::Update(ecs::Manager& manager) {
	auto boxes{ manager.EntitiesWith<BoxCollider>() };
	auto circles{ manager.EntitiesWith<CircleCollider>() };

	for (auto [e1, b1] : boxes) {
		HandleCollisions<BoxCollider>(e1, boxes, circles);
	}

	for (auto [e1, c1] : circles) {
		HandleCollisions<CircleCollider>(e1, boxes, circles);
	}
}

// TODO: Fix or get rid of.
// void CollisionHandler::DrawVelocity(
//	const V2_float& start, const V2_float& vel, const Color& color
//) {
//	Line l{ start, start + vel };
//	//l.Draw(color);
//}

} // namespace ptgn::impl