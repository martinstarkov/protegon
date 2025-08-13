#include "physics/collision/collision_handler.h"

#include <algorithm>
#include <vector>

#include "common/assert.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/entity_hierarchy.h"
#include "core/game.h"
#include "core/manager.h"
#include "core/script.h"
#include "debug/log.h"
#include "ecs/ecs.h"
#include "math/geometry.h"
#include "math/intersect.h"
#include "math/math.h"
#include "math/overlap.h"
#include "math/raycast.h"
#include "math/vector2.h"
#include "physics/collision/bounding_aabb.h"
#include "physics/collision/broadphase.h"
#include "physics/collision/collider.h"
#include "physics/rigid_body.h"
#include "scene/scene.h"
#include "utility/span.h"

namespace ptgn::impl {

bool CollisionHandler::CanCollide(
	const Entity& entity1, const Collider& collider1, const Entity& entity2,
	const Collider& collider2
) {
	if (collider2.mode == CollisionMode::None) {
		return false;
	}
	// Entity collision categories / masks do not match.
	if (!collider1.CanCollideWith(collider2.GetCollisionCategory())) {
		return false;
	}
	const Entity root1{ GetRootEntity(entity1) };
	const Entity root2{ GetRootEntity(entity2) };
	// Entities share the same root entity.
	if (root1 == root2) {
		return false;
	}
	if (!root1.IsAlive() || !root2.IsAlive() || !entity1.IsAlive() || !entity2.IsAlive()) {
		return false;
	}
	return true;
}

template <auto ScriptType, auto EarlyExit>
std::vector<Entity> GetDiscreteCollideables(Entity& entity1, const impl::KDTree& tree) {
	const auto& collider{ entity1.Get<Collider>() };

	Transform transform{ GetAbsoluteTransform(entity1) };

	auto bounding_aabb{ GetBoundingAABB(collider.shape, transform) };

	auto candidates{ tree.Query(bounding_aabb) };

	std::vector<Entity> collideables;
	collideables.reserve(candidates.size());

	for (const auto& entity2 : candidates) {
		if (entity2 == entity1) {
			continue;
		}

		if (!entity1.Has<Collider>()) {
			break;
		}

		if (!entity2.Has<Collider>()) {
			continue;
		}

		const auto& collider2{ entity2.Get<Collider>() };

		if (EarlyExit(collider2)) {
			continue;
		}

		const auto& collider1{ entity1.Get<Collider>() };

		// TODO: Consider if this should always be the case.
		/*if (collider1.CollidedWith(entity2)) {
			continue;
		}*/

		if (!CollisionHandler::CanCollide(entity1, collider1, entity2, collider2)) {
			continue;
		}

		if (const auto scripts{ entity1.TryGet<Scripts>() }) {
			bool can_collide{ scripts->ConditionCheck(ScriptType, entity2) };
			if (can_collide) {
				collideables.emplace_back(entity2);
			}
		} else {
			collideables.emplace_back(entity2);
		}
	}

	return collideables;
}

void CollisionHandler::Overlap(Entity& entity1) const {
	PTGN_ASSERT(entity1.Has<Collider>());
	PTGN_ASSERT(entity1.Get<Collider>().mode == CollisionMode::Overlap);

	auto collideables{ GetDiscreteCollideables<
		&OverlapScript::PreOverlapCheck, [](const Collider& collider2) {
			return collider2.mode == CollisionMode::None;
		}>(entity1, static_tree_) };

	for (const auto& entity2 : collideables) {
		auto transform1{ GetAbsoluteTransform(entity1) };
		auto transform2{ GetAbsoluteTransform(entity2) };

		auto& collider1{ entity1.Get<Collider>() };
		auto& collider2{ entity2.Get<Collider>() };

		auto shape1{ ApplyOffset(collider1.shape, entity1) };
		auto shape2{ ApplyOffset(collider2.shape, entity2) };

		bool overlap{ ptgn::Overlap(transform1, shape1, transform2, shape2) };

		if (!overlap) {
			continue;
		}

		collider1.AddCollision({ entity2, V2_float{} });
		collider2.AddCollision({ entity1, V2_float{} });
	}
}

void CollisionHandler::Intersect(Entity& entity1) const {
	PTGN_ASSERT(entity1.Has<Collider>());

	auto collideables{ GetDiscreteCollideables<
		&CollisionScript::PreCollisionCheck, [](const Collider& collider2) {
			return collider2.mode == CollisionMode::Overlap ||
				   collider2.mode == CollisionMode::None;
		}>(entity1, static_tree_) };

	for (auto& entity2 : collideables) {
		auto transform1{ GetAbsoluteTransform(entity1) };
		auto transform2{ GetAbsoluteTransform(entity2) };

		auto& collider1{ entity1.Get<Collider>() };
		auto& collider2{ entity2.Get<Collider>() };

		auto shape1{ ApplyOffset(collider1.shape, entity1) };
		auto shape2{ ApplyOffset(collider2.shape, entity2) };

		auto intersection{ ptgn::Intersect(transform1, shape1, transform2, shape2) };

		if (!intersection.Occurred()) {
			continue;
		}

		if (auto scripts1{ entity1.TryGet<Scripts>() }) {
			scripts1->AddAction(
				&CollisionScript::OnCollision, Collision{ entity2, intersection.normal }
			);
		}
		if (auto scripts2{ entity2.TryGet<Scripts>() }) {
			scripts2->AddAction(
				&CollisionScript::OnCollision, Collision{ entity1, -intersection.normal }
			);
		}

		collider1.AddCollision(Collision{ entity2, intersection.normal });
		collider2.AddCollision(Collision{ entity1, -intersection.normal });

		if (!entity1.Has<RigidBody>()) {
			continue;
		}

		auto& rigid_body{ entity1.Get<RigidBody>() };

		PhysicsBody body{ entity1 };

		if (body.IsImmovable()) {
			continue;
		}

		// if (entity.Has<Transform>()) {
		// GetPosition(entity) += intersection.normal * (intersection.depth + slop_);
		// }

		auto& root_transform{ body.GetRootTransform() };

		root_transform.position += intersection.normal * (intersection.depth + slop_);

		// TODO: Consider updating both dynamic and static tree here.

		rigid_body.velocity = GetRemainingVelocity(
			rigid_body.velocity, { 0.0f, intersection.normal }, collider1.response
		);
	}
}

std::vector<Entity> CollisionHandler::GetSweepCandidates(
	Entity& entity1, const V2_float& velocity, const KDTree& tree
) {
	const auto& collider{ entity1.Get<Collider>() };

	Transform transform{ GetAbsoluteTransform(entity1) };

	auto bounding_aabb{ GetBoundingAABB(collider.shape, transform) };

	auto candidates{ tree.Raycast(entity1, velocity, bounding_aabb) };

	std::vector<Entity> collideables;
	collideables.reserve(candidates.size());

	for (const auto& entity2 : candidates) {
		PTGN_ASSERT(entity2 != entity1);

		if (!entity1.Has<Collider>() || !entity1.Has<RigidBody>()) {
			break;
		}

		if (!entity2.Has<Collider>()) {
			continue;
		}

		const auto& collider2{ entity2.Get<Collider>() };

		if (collider2.mode == CollisionMode::None || collider2.mode == CollisionMode::Overlap) {
			continue;
		}

		const auto& collider1{ entity1.Get<Collider>() };

		// TODO: Consider if this should always be the case.
		// if (collider1.CollidedWith(entity2)) {
		//	continue;
		//}

		if (!CanCollide(entity1, collider1, entity2, collider2)) {
			continue;
		}

		if (const auto scripts{ entity1.TryGet<Scripts>() }) {
			bool can_collide{
				scripts->ConditionCheck(&CollisionScript::PreCollisionCheck, entity2)
			};
			if (can_collide) {
				collideables.emplace_back(entity2);
			}
		} else {
			collideables.emplace_back(entity2);
		}
	}

	return collideables;
}

std::vector<CollisionHandler::SweepCollision> CollisionHandler::GetSortedCollisions(
	Entity& entity1, const V2_float& offset, const V2_float& velocity1, float dt
) const {
	auto static_collideables{ GetSweepCandidates(entity1, velocity1, static_tree_) };
	auto dynamic_collideables{ GetSweepCandidates(entity1, velocity1, dynamic_tree_) };

	auto collideables{ ConcatenateVectors(static_collideables, dynamic_collideables) };

	std::vector<SweepCollision> collisions;

	for (const auto& entity2 : collideables) {
		if (entity1 == entity2) {
			continue;
		}

		auto transform1{ GetAbsoluteTransform(entity1) };
		auto transform2{ GetAbsoluteTransform(entity2) };

		auto offset_transform{ transform1 };
		offset_transform.position += offset;

		const auto& collider1{ entity1.Get<Collider>() };
		const auto& collider2{ entity2.Get<Collider>() };

		auto shape1{ ApplyOffset(collider1.shape, entity1) };
		auto shape2{ ApplyOffset(collider2.shape, entity2) };

		auto relative_velocity{ GetRelativeVelocity(velocity1, entity2, dt) };

		auto raycast{
			ptgn::Raycast(relative_velocity, offset_transform, shape1, transform2, shape2)
		};

		if (!raycast.Occurred()) {
			continue;
		}

		auto center1{ offset_transform.position };
		auto center2{ transform2.position };
		V2_float center_dist{ center1 - center2 };
		float dist2{ center_dist.MagnitudeSquared() };
		collisions.emplace_back(raycast, dist2, entity2);
	}

	SortCollisions(collisions);

	return collisions;
}

void CollisionHandler::Sweep(Entity& entity, float dt) {
	PTGN_ASSERT(entity.Has<Collider>());
	PTGN_ASSERT(entity.Get<Collider>().mode == CollisionMode::Sweep);
	PTGN_ASSERT(entity.Has<RigidBody>());

	std::size_t iterations{ 0 };

	V2_float offset;

	do {
		auto velocity{ entity.Get<RigidBody>().velocity * dt };

		if (velocity.IsZero()) {
			break;
		}

		auto collisions{ GetSortedCollisions(entity, offset, velocity, dt) };

		if (collisions.empty()) {
			break;
		}

		// no collisions occured.
		// TODO: Fix or get rid of.
		/*if (debug_draw) {
			DrawDebugLine(transform.position, velocity, color::Gray);
		}*/
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

		AddEarliestCollisions(entity, collisions);

		entity.Get<RigidBody>().velocity *= earliest.t;

		auto new_velocity{
			GetRemainingVelocity(velocity, earliest, entity.Get<Collider>().response)
		};

		// TODO: Check if this is even needed.
		auto transform{ GetAbsoluteTransform(entity) };
		auto new_bounding_aabb{ GetBoundingAABB(entity.Get<Collider>().shape, transform) };
		const auto& rb{ entity.Get<RigidBody>() };
		auto v{ new_velocity };
		auto new_expanded_aabb{ new_bounding_aabb.ExpandByVelocity(v) };
		dynamic_tree_.UpdateBoundingAABB(entity, new_expanded_aabb);
		dynamic_tree_.EndFrameUpdate();

		if (new_velocity.IsZero()) {
			break;
		}

		offset += velocity * earliest.t;

		auto collisions2{ GetSortedCollisions(entity, offset, new_velocity, dt) };

		PTGN_ASSERT(dt > 0.0f);

		if (collisions2.empty()) {
			// TODO: Fix or get rid of.
			/*if (debug_draw) {
				DrawDebugLine(
					transform.position + velocity * earliest.t, new_velocity, color::Orange
				);
			}*/

			entity.Get<RigidBody>().AddImpulse(new_velocity / dt);
			break;
		}

		auto earliest2{ collisions2.front().collision };

		// TODO: Fix or get rid of.
		/*if (debug_draw) {
			DrawDebugLine(
				transform.position + velocity * earliest.t, new_velocity * earliest2.t, color::Green
			);
		}*/

		AddEarliestCollisions(entity, collisions2);

		entity.Get<RigidBody>().AddImpulse(new_velocity / dt * earliest2.t);

		iterations++;
	} while (iterations < max_sweep_iterations_);

	// TODO: Consider adding.
	// Intersect(entity);
}

V2_float CollisionHandler::GetRelativeVelocity(
	const V2_float& velocity1, const Entity& entity2, float dt
) {
	V2_float relative_velocity{ velocity1 };
	if (const auto rb2{ entity2.TryGet<RigidBody>() }) {
		auto velocity2{ rb2->velocity * dt };
		relative_velocity -= velocity2;
	}
	return relative_velocity;
}

void CollisionHandler::AddEarliestCollisions(
	Entity& entity, const std::vector<SweepCollision>& sweep_collisions
) {
	PTGN_ASSERT(!sweep_collisions.empty());

	const auto& first_sweep{ sweep_collisions.front() };

	PTGN_ASSERT(entity != first_sweep.entity, "Self collision not possible");

	Collision first{ first_sweep.entity, first_sweep.collision.normal };

	auto& collider{ entity.Get<Collider>() };

	auto scripts{ entity.TryGet<Scripts>() };

	if (scripts) {
		scripts->AddAction(&CollisionScript::OnCollision, first);
	}
	collider.AddCollision(first);

	for (std::size_t i{ 1 }; i < sweep_collisions.size(); ++i) {
		const auto& sweep{ sweep_collisions[i] };

		if (sweep.collision.t == first_sweep.collision.t) {
			PTGN_ASSERT(entity != sweep.entity, "Self collision not possible");
			Collision matching{ sweep.entity, sweep.collision.normal };
			if (scripts) {
				scripts->AddAction(&CollisionScript::OnCollision, matching);
			}
			collider.AddCollision(matching);
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
	std::ranges::sort(collisions, [](const SweepCollision& a, const SweepCollision& b) {
		return a.dist2 < b.dist2;
	});
	// Sort based on collision times, and if they are equal, by collision normal magnitudes.
	std::ranges::sort(collisions, [](const SweepCollision& a, const SweepCollision& b) {
		// If time of collision are equal, prioritize walls to corners, i.e. normals
		// (1,0) come before (1,1).
		if (a.collision.t == b.collision.t) {
			return a.collision.normal.MagnitudeSquared() < b.collision.normal.MagnitudeSquared();
		}
		// If collision times are not equal, sort by collision time.
		return a.collision.t < b.collision.t;
	});
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
	std::vector<KDObject> objects;
	std::vector<KDObject> dynamic_objects;

	float dt{ game.dt() };

	for (auto [entity, collider] : scene.EntitiesWith<Collider>()) {
		collider.ResetCollisions();
		auto transform{ GetAbsoluteTransform(entity) };
		auto bounding_aabb{ GetBoundingAABB(collider.shape, transform) };
		objects.emplace_back(entity, bounding_aabb);
		if (entity.Has<RigidBody>()) {
			const auto& rb{ entity.Get<RigidBody>() };
			auto velocity{ rb.velocity * dt };
			auto expanded_aabb{ bounding_aabb.ExpandByVelocity(velocity) };
			dynamic_objects.emplace_back(entity, expanded_aabb);
		}
	}

	static_tree_.Build(objects);
	dynamic_tree_.Build(dynamic_objects);

	for (auto& object : objects) {
		const auto& collider{ object.entity.Get<Collider>() };
		switch (collider.mode) {
			case CollisionMode::Intersect: {
				Intersect(object.entity);
				break;
			}
			case CollisionMode::Overlap: {
				Overlap(object.entity);
				break;
			}
			case CollisionMode::Sweep: {
				if (!object.entity.Has<RigidBody>()) {
					break;
				}
				Sweep(object.entity, dt);
				break;
			}
			case CollisionMode::None: {
				break;
			}
			default: PTGN_ERROR("Unknown collision mode");
		}
	}

	for (auto [entity, collider, scripts] : scene.EntitiesWith<Collider, Scripts>()) {
		if (collider.mode != CollisionMode::Overlap) {
			continue;
		}
		for (const auto& current : collider.collisions_) {
			PTGN_ASSERT(current.entity != entity);
			if (!VectorContains(collider.prev_collisions_, current)) {
				scripts.AddAction(&OverlapScript::OnOverlapStart, current.entity);
			}
		}
		for (const auto& previous : collider.prev_collisions_) {
			PTGN_ASSERT(previous.entity != entity);
			if (!VectorContains(collider.collisions_, previous)) {
				scripts.AddAction(&OverlapScript::OnOverlapStop, previous.entity);
			} else {
				scripts.AddAction(&OverlapScript::OnOverlap, previous.entity);
			}
		}
	}

	for (auto [entity, collider, scripts] : scene.EntitiesWith<Collider, Scripts>()) {
		scripts.InvokeActions();
	}

	scene.Refresh();
}

CollisionHandler::SweepCollision::SweepCollision(
	const RaycastResult& raycast_result, float distance_squared, Entity sweep_entity
) :
	entity{ sweep_entity }, collision{ raycast_result }, dist2{ distance_squared } {}

} // namespace ptgn::impl