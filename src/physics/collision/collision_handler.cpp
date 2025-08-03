#include "physics/collision/collision_handler.h"

#include <algorithm>
#include <vector>

#include "common/assert.h"
#include "components/common.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "debug/log.h"
#include "ecs/ecs.h"
#include "math/geometry.h"
#include "math/intersect.h"
#include "math/math.h"
#include "math/overlap.h"
#include "math/raycast.h"
#include "math/vector2.h"
#include "physics/collision/collider.h"
#include "physics/rigid_body.h"
#include "scene/scene.h"

namespace ptgn::impl {

bool CollisionHandler::CanCollide(const Entity& entity1, const Entity& entity2) {
	PTGN_ASSERT(entity1.IsEnabled() && entity2.IsEnabled());
	if (entity1.GetRootEntity() == entity2.GetRootEntity()) {
		return false;
	}
	if (!entity1.GetRootEntity().IsAlive()) {
		return false;
	}
	if (!entity2.GetRootEntity().IsAlive()) {
		return false;
	}
	if (!entity1.Get<Collider>().CanCollideWith(entity2.Get<Collider>().GetCollisionCategory())) {
		return false;
	}
	return true;
}

void CollisionHandler::Overlap(Entity entity, const std::vector<Entity>& entities) {
	// Optional: Make overlaps only work one way.
	if (!entity.Get<Collider>().overlap_only) {
		return;
	}

	for (const auto& entity2 : entities) {
		if (!CanCollide(entity, entity2)) {
			continue;
		}
		// ProcessCallback may invalidate all component references.

		if (!entity.Get<Collider>().ProcessCallback(entity, entity2) ||
			!entity2.Get<Collider>().ProcessCallback(entity2, entity)) {
			continue;
		}

		auto transform1{ entity.GetAbsoluteTransform() };
		auto transform2{ entity2.GetAbsoluteTransform() };

		auto shape1{ ApplyOffset(entity.Get<Collider>().shape, entity) };
		auto shape2{ ApplyOffset(entity2.Get<Collider>().shape, entity2) };

		if (!ptgn::Overlap(transform1, shape1, transform2, shape2)) {
			continue;
		}

		auto& collider{ entity.Get<Collider>() };
		collider.AddCollision(Collision{ entity2, V2_float{} });

		auto& collider2{ entity2.Get<Collider>() };
		collider2.AddCollision(Collision{ entity, V2_float{} });
	}
}

void CollisionHandler::Intersect(Entity entity, const std::vector<Entity>& entities) {
	if (entity.Get<Collider>().overlap_only) {
		return;
	}

	for (const auto& entity2 : entities) {
		if (entity2.Get<Collider>().overlap_only || !CanCollide(entity, entity2)) {
			continue;
		}

		// ProcessCallback may invalidate all component references.
		if (!entity.Get<Collider>().ProcessCallback(entity, entity2) ||
			!entity2.Get<Collider>().ProcessCallback(entity2, entity)) {
			continue;
		}

		auto transform1{ entity.GetAbsoluteTransform() };
		auto transform2{ entity2.GetAbsoluteTransform() };

		auto shape1{ ApplyOffset(entity.Get<Collider>().shape, entity) };
		auto shape2{ ApplyOffset(entity2.Get<Collider>().shape, entity2) };

		auto intersection{ ptgn::Intersect(transform1, shape1, transform2, shape2) };

		if (!intersection.Occurred()) {
			continue;
		}

		auto& collider{ entity.Get<Collider>() };
		collider.AddCollision(Collision{ entity2, intersection.normal });

		auto& collider2{ entity2.Get<Collider>() };
		collider2.AddCollision(Collision{ entity, -intersection.normal });

		if (!entity.Has<RigidBody>()) {
			continue;
		}

		auto& rigid_body{ entity.Get<RigidBody>() };

		PhysicsBody body{ entity };

		if (body.IsImmovable()) {
			continue;
		}
		/*if (entity.Has<Transform>()) {
			entity.GetPosition() +=
				intersection.normal * (intersection.depth + slop);
		}*/
		auto& root_transform{ body.GetRootTransform() };

		root_transform.position += intersection.normal * (intersection.depth + slop);

		rigid_body.velocity = GetRemainingVelocity(
			rigid_body.velocity, { 0.0f, intersection.normal }, entity.Get<Collider>().response
		);
	}
}

// Updates the velocity of the object to prevent it from colliding with the target objects.
void CollisionHandler::Sweep(
	Entity entity,
	const std::vector<Entity>& entities /* TODO: Fix or get rid of: , bool debug_draw = false */
) {
	if (const auto& collider{ entity.Get<Collider>() };
		!collider.continuous || collider.overlap_only || !entity.Has<RigidBody>()) {
		return;
	}

	auto velocity{ entity.Get<RigidBody>().velocity * game.dt() };

	if (velocity.IsZero()) {
		return;
	}

	auto collisions{ GetSortedCollisions(entity, entities, {}, velocity) };

	if (collisions.empty()) { // no collisions occured.
		// TODO: Fix or get rid of.
		/*if (debug_draw) {
			DrawDebugLine(transform.position, velocity, color::Gray);
		}*/
		return;
	}

	auto earliest{ collisions.front().collision };

	// TODO: Fix or get rid of.
	/*if (debug_draw) {
		DrawDebugLine(transform.position, velocity * earliest.t, color::Blue);
		if constexpr (std::is_same_v<T, BoxCollider>) {
			Rect rect{ transform.position + velocity * earliest.t, collider.size,
					   collider.origin };
			rect.Draw(color::Purple);
		} else if constexpr (std::is_same_v<T, CircleCollider>) {
			Circle circle{ transform.position + velocity * earliest.t, collider.radius };
			circle.Draw(color::Purple);
		}
	}*/

	AddEarliestCollisions(entity, collisions, entity.Get<Collider>());

	entity.Get<RigidBody>().velocity *= earliest.t;

	auto new_velocity{ GetRemainingVelocity(velocity, earliest, entity.Get<Collider>().response) };

	if (new_velocity.IsZero()) {
		return;
	}

	auto collisions2{ GetSortedCollisions(entity, entities, velocity * earliest.t, new_velocity) };

	PTGN_ASSERT(game.dt() > 0.0f);

	if (collisions2.empty()) {
		// TODO: Fix or get rid of.
		/*if (debug_draw) {
			DrawDebugLine(
				transform.position + velocity * earliest.t, new_velocity, color::Orange
			);
		}*/

		entity.Get<RigidBody>().AddImpulse(new_velocity / game.dt());
		return;
	}

	auto earliest2{ collisions2.front().collision };

	// TODO: Fix or get rid of.
	/*if (debug_draw) {
		DrawDebugLine(
			transform.position + velocity * earliest.t, new_velocity * earliest2.t, color::Green
		);
	}*/

	AddEarliestCollisions(entity, collisions2, entity.Get<Collider>());

	entity.Get<RigidBody>().AddImpulse(new_velocity / game.dt() * earliest2.t);
}

V2_float CollisionHandler::GetRelativeVelocity(const V2_float& velocity, Entity entity2) {
	V2_float relative_velocity{ velocity };
	if (entity2.Has<RigidBody>()) {
		// TODO: Use scene.physics.dt() here and elsewhere.
		relative_velocity -= entity2.Get<RigidBody>().velocity * game.dt();
	}
	return relative_velocity;
}

void CollisionHandler::AddEarliestCollisions(
	Entity entity, const std::vector<SweepCollision>& sweep_collisions, Collider& collider
) {
	PTGN_ASSERT(!sweep_collisions.empty());

	const auto& first_sweep{ sweep_collisions.front() };

	PTGN_ASSERT(entity != first_sweep.entity, "Self collision not possible");

	collider.AddCollision(Collision{ first_sweep.entity, first_sweep.collision.normal });

	for (std::size_t i{ 1 }; i < sweep_collisions.size(); ++i) {
		const auto& sweep{ sweep_collisions[i] };

		if (sweep.collision.t == first_sweep.collision.t) {
			PTGN_ASSERT(entity != sweep.entity, "Self collision not possible");
			collider.AddCollision(Collision{ sweep.entity, sweep.collision.normal });
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
	auto entities{ scene.EntitiesWith<Enabled, Collider>().GetVector() };

	for (const auto& entity1 : entities) {
		entity1.Get<Collider>().ResetCollisions();
	}

	for (const auto& entity1 : entities) {
		HandleCollisions(entity1, entities);
	}

	scene.Refresh();
}

std::vector<CollisionHandler::SweepCollision> CollisionHandler::GetSortedCollisions(
	Entity entity, const std::vector<Entity>& entities, const V2_float& offset, const V2_float& vel
) {
	std::vector<SweepCollision> collisions;

	for (const auto& entity2 : entities) {
		if (entity2.Get<Collider>().overlap_only || !CanCollide(entity, entity2)) {
			continue;
		}

		// ProcessCallback may invalidate all component references.
		if (!entity.Get<Collider>().ProcessCallback(entity, entity2) ||
			!entity2.Get<Collider>().ProcessCallback(entity2, entity)) {
			continue;
		}

		auto transform1{ entity.GetAbsoluteTransform() };
		auto transform2{ entity2.GetAbsoluteTransform() };

		auto offset_transform{ transform1 };
		offset_transform.position += offset;

		auto shape1{ ApplyOffset(entity.Get<Collider>().shape, entity) };
		auto shape2{ ApplyOffset(entity2.Get<Collider>().shape, entity2) };

		auto raycast{ ptgn::Raycast(
			GetRelativeVelocity(vel, entity2), offset_transform, shape1, transform2, shape2
		) };

		if (raycast.Occurred()) {
			auto center1{ offset_transform.position };
			auto center2{ transform2.position };
			auto dist2{ (center1 - center2).MagnitudeSquared() };
			collisions.emplace_back(raycast, dist2, entity2);
		}
	}

	SortCollisions(collisions);

	return collisions;
}

void CollisionHandler::HandleCollisions(Entity entity, const std::vector<Entity>& entities) {
	auto& collider{ entity.Get<Collider>() };

	PTGN_ASSERT(entity.IsEnabled());

	Intersect(entity, entities);
	Sweep(entity, entities);
	Overlap(entity, entities);

	collider = entity.Get<Collider>();

	for (const auto& prev : collider.prev_collisions_) {
		PTGN_ASSERT(entity != prev.entity);
	}
	for (const auto& current : collider.collisions_) {
		PTGN_ASSERT(entity != current.entity);
	}

	collider.InvokeCollisionCallbacks(entity);
}

CollisionHandler::SweepCollision::SweepCollision(
	const RaycastResult& raycast_result, float distance_squared, Entity sweep_entity
) :
	entity{ sweep_entity }, collision{ raycast_result }, dist2{ distance_squared } {}

} // namespace ptgn::impl