#include "runtime/physics/collision_handler.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/intersect.h"
#include "core/math/math_utils.h"
#include "core/math/overlap.h"
#include "core/math/raycast.h"
#include "core/math/tolerance.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/span.h"
#include "core/util/time.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/shape.h"
#include "runtime/physics/bounding_aabb.h"
#include "runtime/physics/broadphase.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/collision.h"
#include "runtime/physics/collision_event.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_event.h"
#include "runtime/scripting/script.h"
#include "tools/debug/debug_system.h"

namespace ptgn {

bool CollisionHandler::CanCollide(
	Entity entity1, const Collider& collider1, Entity entity2, const Collider& collider2
) {
	if (collider2.mode == CollisionMode::None) {
		return false;
	}
	// Entity collision categories / masks do not match.
	if (!collider1.CanCollideWith(collider2.GetMask())) {
		return false;
	}
	const Entity root1{ GetRootEntity(entity1) };
	const Entity root2{ GetRootEntity(entity2) };
	// Entities share the same root entity.
	if (root1 == root2) {
		return false;
	}
	if (!root1 || !root2 || !entity1 || !entity2) {
		return false;
	}
	return true;
}

template <auto EarlyExit, bool kPreOverlapCheck>
std::vector<Entity> GetDiscreteCollideables(Entity entity1, const impl::KDTree& tree) {
	const auto& collider{ entity1.Get<Collider>() };

	Transform transform{ GetWorldTransform(entity1) };
	transform = OffsetByOrigin(collider.shape, transform, entity1);

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

		const auto& collider1{ entity1.Get<Collider>() };
		const auto& collider2{ entity2.Get<Collider>() };

		if (EarlyExit(entity1, collider1, entity2, collider2)) {
			continue;
		}

		if (!CollisionHandler::CanCollide(entity1, collider1, entity2, collider2)) {
			continue;
		}

		if (auto collider1comp{ entity1.TryGet<Collider>() }) {
			bool can_collide{ true };
			if constexpr (kPreOverlapCheck) {
				if (collider1comp->pre_overlap_check) {
					can_collide = std::invoke(collider1comp->pre_overlap_check, entity1, entity2);
				}
			} else {
				if (collider1comp->pre_collision_check) {
					can_collide = std::invoke(collider1comp->pre_collision_check, entity1, entity2);
				}
			}
			if (can_collide) {
				collideables.emplace_back(entity2);
			}
		} else {
			collideables.emplace_back(entity2);
		}
	}

	return collideables;
}

void CollisionHandler::UpdateKDTree(Entity entity, secondsf dt) {
	const auto& collider{ entity.Get<Collider>() };
	auto transform{ GetWorldTransform(entity) };
	transform = OffsetByOrigin(collider.shape, transform, entity);
	const auto new_bounding_aabb{ GetBoundingAABB(collider.shape, transform) };
	static_tree_.UpdateBoundingAABB(entity, new_bounding_aabb);
	static_tree_.EndFrameUpdate();
	if (const auto rb{ entity.TryGet<RigidBody>() }) {
		auto v{ rb->velocity * dt.count() };
		auto new_expanded_aabb{ new_bounding_aabb.ExpandByVelocity(v) };
		dynamic_tree_.UpdateBoundingAABB(entity, new_expanded_aabb);
		dynamic_tree_.EndFrameUpdate();
	}
}

void CollisionHandler::Overlap(Entity entity1) const {
	PTGN_ASSERT(entity1.Has<Collider>());
	PTGN_ASSERT(entity1.Get<Collider>().mode == CollisionMode::Overlap);

	auto collideables{ GetDiscreteCollideables<
		[]([[maybe_unused]] Entity e1, const Collider& c1, Entity e2, const Collider& c2) {
			return c2.mode == CollisionMode::None || c1.OverlappedWith(e2);
		},
		true>(entity1, static_tree_) };

	for (const auto& entity2 : collideables) {
		auto& collider1{ entity1.Get<Collider>() };
		auto& collider2{ entity2.Get<Collider>() };

		auto t1{ GetWorldTransform(entity1) };
		auto t2{ GetWorldTransform(entity2) };

		auto transform1{ OffsetByOrigin(collider1.shape, t1, entity1) };
		auto transform2{ OffsetByOrigin(collider2.shape, t2, entity2) };

		if (!ptgn::Overlap(transform1, collider1.shape, transform2, collider2.shape)) {
			continue;
		}

		collider1.AddOverlap(entity2);
		collider2.AddOverlap(entity1);
	}
}

void CollisionHandler::Intersect(Entity entity1, secondsf dt) {
	PTGN_ASSERT(entity1.Has<Collider>());

	auto collideables{ GetDiscreteCollideables<
		[]([[maybe_unused]] Entity e1, [[maybe_unused]] const Collider& c1,
		   [[maybe_unused]] Entity e2, const Collider& c2) {
			return c2.mode == CollisionMode::Overlap ||
				   c2.mode == CollisionMode::None; //|| c1.IntersectedWith(e2);
		},
		false>(entity1, static_tree_) };

	std::vector<Entity> moved_entities;

	for (auto& entity2 : collideables) {
		auto& collider1{ entity1.Get<Collider>() };
		auto& collider2{ entity2.Get<Collider>() };

		auto t1{ GetWorldTransform(entity1) };
		auto t2{ GetWorldTransform(entity2) };

		auto transform1{ OffsetByOrigin(collider1.shape, t1, entity1) };
		auto transform2{ OffsetByOrigin(collider2.shape, t2, entity2) };

		auto intersection{
			ptgn::Intersect(transform1, collider1.shape, transform2, collider2.shape)
		};

		if (!intersection.Occurred()) {
			continue;
		}

		PushEvent<event::Collision>(entity1, CollisionInfo{ entity2, intersection.normal });
		PushEvent<event::Collision>(entity2, CollisionInfo{ entity1, -intersection.normal });

		collider1.AddIntersect(CollisionInfo{ entity2, intersection.normal });
		collider2.AddIntersect(CollisionInfo{ entity1, -intersection.normal });

		if (!entity1.Has<RigidBody>()) {
			continue;
		}

		if (IsImmovable(entity1)) {
			continue;
		}

		Entity root_entity{ GetRootEntity(entity1) };

		auto& root_transform{ root_entity.Get<Transform>() };

		auto minimum_translation_vector{ intersection.normal * (intersection.depth + slop_) };

		root_transform.Translate(minimum_translation_vector);

		moved_entities.emplace_back(entity1);

		if (auto rigid_body{ root_entity.TryGet<RigidBody>() }) {
			rigid_body->velocity = GetRemainingVelocity(
				rigid_body->velocity, { 0.0f, intersection.normal }, collider1.response
			);
		}
	}

	for (const auto& entity : moved_entities) {
		UpdateKDTree(entity, dt);
	}
}

std::vector<Entity> CollisionHandler::GetSweepCandidates(
	Entity entity1, V2_float velocity, const impl::KDTree& tree
) {
	const auto& collider{ entity1.Get<Collider>() };

	Transform transform{ GetWorldTransform(entity1) };
	transform = OffsetByOrigin(collider.shape, transform, entity1);

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

		/*if (collider1.SweptWith(entity2)) {
			continue;
		}*/

		if (!CanCollide(entity1, collider1, entity2, collider2)) {
			continue;
		}

		if (auto collider_comp{ entity1.TryGet<Collider>() }) {
			bool can_collide{ true };
			if (collider_comp->pre_collision_check) {
				can_collide = std::invoke(collider_comp->pre_collision_check, entity1, entity2);
			}
			if (can_collide) {
				collideables.emplace_back(entity2);
			}
		} else {
			collideables.emplace_back(entity2);
		}
	}

	return collideables;
}

std::vector<impl::SweepCollision> CollisionHandler::GetSortedCollisions(
	Entity entity1, V2_float offset, V2_float velocity1, secondsf dt
) const {
	auto static_collideables{ GetSweepCandidates(entity1, velocity1, static_tree_) };
	auto dynamic_collideables{ GetSweepCandidates(entity1, velocity1, dynamic_tree_) };

	auto collideables{ VectorConcat(static_collideables, dynamic_collideables) };

	VectorRemoveDuplicates(collideables);

	std::vector<impl::SweepCollision> collisions;

	for (const auto& entity2 : collideables) {
		if (entity1 == entity2) {
			continue;
		}

		auto t1{ GetWorldTransform(entity1) };
		auto t2{ GetWorldTransform(entity2) };

		Transform offset_transform{ t1 };
		offset_transform.Translate(offset);

		const auto& collider1{ entity1.Get<Collider>() };
		const auto& collider2{ entity2.Get<Collider>() };

		auto transform1{ OffsetByOrigin(collider1.shape, offset_transform, entity1) };
		auto transform2{ OffsetByOrigin(collider2.shape, t2, entity2) };

		auto relative_velocity{ GetRelativeVelocity(velocity1, entity2, dt) };

		auto raycast{ ptgn::Raycast(
			relative_velocity, transform1, collider1.shape, transform2, collider2.shape
		) };

		if (!raycast.Occurred()) {
			continue;
		}

		auto center1{ transform1.GetPosition() };
		auto center2{ transform2.GetPosition() };
		V2_float center_dist{ center1 - center2 };
		float dist2{ center_dist.MagnitudeSquared() };
		collisions.emplace_back(raycast, dist2, entity2);
	}

	SortCollisions(collisions);

	return collisions;
}

void CollisionHandler::Sweep(Scene& scene, Entity entity, secondsf dt) {
	PTGN_ASSERT(entity.Has<Collider>());
	PTGN_ASSERT(entity.Get<Collider>().mode == CollisionMode::Continuous);
	PTGN_ASSERT(entity.Has<RigidBody>());

	[[maybe_unused]] std::size_t iterations{ 0 };

	V2_float offset;

	bool raycast_hit{ false };

	do {
		auto velocity{ entity.Get<RigidBody>().velocity * dt.count() };

		if (velocity.IsZero()) {
			break;
		}

		auto collisions{ GetSortedCollisions(entity, offset, velocity, dt) };

		if (collisions.empty()) {
			break;
		}

		raycast_hit = true;

		// no collisions occured.
		TryDrawDebugLine(scene, entity, {}, velocity, color::Gray);

		auto earliest{ collisions.front().collision };

		TryDrawDebugLine(scene, entity, {}, velocity * earliest.t, color::Blue);
		TryDrawDebugCollider(scene, entity, velocity * earliest.t, color::Purple);

		AddEarliestCollisions(entity, collisions);

		entity.Get<RigidBody>().velocity *= earliest.t;

		auto new_velocity{
			GetRemainingVelocity(velocity, earliest, entity.Get<Collider>().response)
		};

		if (new_velocity.IsZero()) {
			break;
		}

		offset += velocity * earliest.t;

		auto collisions2{ GetSortedCollisions(entity, offset, new_velocity, dt) };

		PTGN_ASSERT(dt > 0s);

		if (collisions2.empty()) {
			TryDrawDebugLine(scene, entity, velocity * earliest.t, new_velocity, color::Orange);

			entity.Get<RigidBody>().AddImpulse(new_velocity / dt.count());
			break;
		}

		auto earliest2{ collisions2.front().collision };

		TryDrawDebugLine(
			scene, entity, velocity * earliest.t, new_velocity * earliest2.t, color::Green
		);

		AddEarliestCollisions(entity, collisions2);

		entity.Get<RigidBody>().AddImpulse(new_velocity / dt.count() * earliest2.t);

		iterations++;
	} while (false /*TODO: Consider readding: iterations < max_sweep_iterations_*/);

	if (raycast_hit) {
		// TODO: Check if this is even needed.
		UpdateKDTree(entity, dt);
	}
}

void CollisionHandler::TryDrawDebugCollider(
	Scene& scene, Entity entity, V2_float offset, Color color
) const {
	if (debug_settings_.DrawCCD()) {
		auto transform{ GetWorldTransform(entity) };
		transform.Translate(offset);
		const auto& collider{ entity.Get<Collider>() };
		scene.ctx().debug.DrawShape(collider.shape, transform, color);
	}
}

void CollisionHandler::TryDrawDebugLine(
	Scene& scene, Entity entity, V2_float start_offset, V2_float end_offset, Color color
) const {
	if (debug_settings_.DrawCCD()) {
		auto transform{ GetWorldTransform(entity) };
		auto position{ transform.GetPosition() };
		scene.ctx().debug.DrawLine(position + start_offset, position + end_offset, color);
	}
}

V2_float CollisionHandler::GetRelativeVelocity(V2_float velocity1, Entity entity2, secondsf dt) {
	V2_float relative_velocity{ velocity1 };
	if (const auto rb2{ entity2.TryGet<RigidBody>() }) {
		auto velocity2{ rb2->velocity * dt.count() };
		relative_velocity -= velocity2;
	}
	return relative_velocity;
}

void CollisionHandler::AddEarliestCollisions(
	Entity entity, const std::vector<impl::SweepCollision>& sweep_collisions
) {
	PTGN_ASSERT(!sweep_collisions.empty());

	const auto& first_sweep{ sweep_collisions.front() };

	PTGN_ASSERT(entity != first_sweep.entity, "Self collision not possible");

	CollisionInfo first{ first_sweep.entity, first_sweep.collision.normal };

	auto& collider{ entity.Get<Collider>() };

	PushEvent<event::Collision>(entity, first);

	collider.AddSweep(first);

	for (auto i{ 1uz }; i < sweep_collisions.size(); ++i) {
		const auto& sweep{ sweep_collisions[i] };

		if (sweep.collision.t == first_sweep.collision.t) {
			PTGN_ASSERT(entity != sweep.entity, "Self collision not possible");
			CollisionInfo matching{ sweep.entity, sweep.collision.normal };

			PushEvent<event::Collision>(entity, matching);

			collider.AddSweep(matching);
		}
	}
};

void CollisionHandler::SortCollisions(std::vector<impl::SweepCollision>& collisions) {
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
	std::ranges::sort(collisions, [](const impl::SweepCollision& a, const impl::SweepCollision& b) {
		return a.dist2 < b.dist2;
	});
	// Sort based on collision times, and if they are equal, by collision normal magnitudes.
	std::ranges::sort(collisions, [](const impl::SweepCollision& a, const impl::SweepCollision& b) {
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
	V2_float velocity, const RaycastResult& collision, CollisionResponse response
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
			auto abs_normal{ Abs(collision.normal) };
			if (!NearlyEqual(abs_normal.x, 0.0f)) {
				new_velocity.x *= -1.0f;
			}
			if (!NearlyEqual(abs_normal.y, 0.0f)) {
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

void CollisionHandler::Update(Scene& scene, secondsf dt) {
	std::vector<impl::KDObject> objects;
	std::vector<impl::KDObject> dynamic_objects;

	for (auto [entity, collider] : scene.EntitiesWith<Collider>()) {
		collider.ResetContainers();
		auto transform{ GetWorldTransform(entity) };
		transform = OffsetByOrigin(collider.shape, transform, entity);
		auto bounding_aabb{ GetBoundingAABB(collider.shape, transform) };
		objects.emplace_back(entity, bounding_aabb);
		if (entity.Has<RigidBody>()) {
			const auto& rb{ entity.Get<RigidBody>() };
			auto velocity{ rb.velocity * dt.count() };
			auto expanded_aabb{ bounding_aabb.ExpandByVelocity(velocity) };
			dynamic_objects.emplace_back(entity, expanded_aabb);
		}
	}

	static_tree_.Build(objects);
	dynamic_tree_.Build(dynamic_objects);

	for (auto& object : objects) {
		const auto& collider{ object.entity.Get<Collider>() };
		switch (collider.mode) {
			case CollisionMode::Discrete: {
				Intersect(object.entity, dt);
				break;
			}
			case CollisionMode::Overlap: {
				Overlap(object.entity);
				break;
			}
			case CollisionMode::Continuous: {
				if (!object.entity.Has<RigidBody>()) {
					break;
				}
				// Ensure the collider does not start within an object (at least most of
				// the time).
				Intersect(object.entity, dt);
				Sweep(scene, object.entity, dt);
				break;
			}
			case CollisionMode::None: {
				break;
			}
			default: PTGN_ERROR("Unknown collision mode");
		}
	}

	for (auto [entity, collider, _scripts] : scene.EntitiesWith<Collider, impl::Scripts>()) {
		if (collider.mode != CollisionMode::Overlap) {
			continue;
		}
		for (const auto& current : collider.overlaps_) {
			PTGN_ASSERT(current != entity);
			if (!std::ranges::contains(collider.previous_overlaps_, current)) {
				PushEvent<event::OverlapStart>(entity, current);
			}
		}
		for (const auto& previous : collider.previous_overlaps_) {
			PTGN_ASSERT(previous != entity);
			if (!std::ranges::contains(collider.overlaps_, previous)) {
				PushEvent<event::OverlapStop>(entity, previous);
			} else {
				PushEvent<event::Overlap>(entity, previous);
			}
		}
	}
}

void CollisionHandler::SetDebugSettings(const CollisionDebugSettings& settings) {
	debug_settings_ = settings;
}

const CollisionDebugSettings& CollisionHandler::GetDebugSettings() const {
	return debug_settings_;
}

namespace impl {

SweepCollision::SweepCollision(
	const RaycastResult& raycast_result, float distance_squared, Entity sweep_entity
) :
	entity{ sweep_entity }, collision{ raycast_result }, dist2{ distance_squared } {}

} // namespace impl

} // namespace ptgn