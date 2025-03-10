#pragma once

#include <unordered_set>
#include <vector>

#include "components/transform.h"
#include "core/game.h"
#include "core/game_object.h"
#include "ecs/ecs.h"
#include "math/collision/collider.h"
#include "math/collision/intersect.h"
#include "math/collision/overlap.h"
#include "math/collision/raycast.h"
#include "math/geometry/circle.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "physics/rigid_body.h"
#include "utility/assert.h"

namespace ptgn {

class Scene;

namespace impl {

class Game;
class Physics;

class CollisionHandler {
public:
	CollisionHandler()										 = default;
	~CollisionHandler()										 = default;
	CollisionHandler(CollisionHandler&&) noexcept			 = default;
	CollisionHandler& operator=(CollisionHandler&&) noexcept = default;
	CollisionHandler& operator=(const CollisionHandler&)	 = delete;
	CollisionHandler(const CollisionHandler&)				 = delete;

	template <typename T, typename S>
	[[nodiscard]] static bool CanCollide(
		const GameObject e1, const T& colliderA, const GameObject e2, const S& colliderB
	) {
		if (!e1.IsEnabled()) {
			return false;
		}
		if (!e2.IsEnabled()) {
			return false;
		}
		// TODO: Check if the "oldest" parents are equal.
		if (e1.GetParent() == e2.GetParent()) {
			return false;
		}
		if (!e1.GetParent().IsAlive()) {
			return false;
		}
		if (!e2.GetParent().IsAlive()) {
			return false;
		}
		if (!colliderA.CanCollideWith(colliderB.GetCollisionCategory())) {
			return false;
		}
		return true;
	}

	template <typename T>
	static void Overlap(
		ecs::Entity entity, const ecs::EntitiesWith<BoxCollider>& boxes,
		const ecs::EntitiesWith<CircleCollider>& circles
	) {
		if (!entity.Get<T>().overlap_only) {
			return;
		}

		const auto process_overlap = [&](const auto& collider2, ecs::Entity e2) {
			const auto& collider{ entity.Get<T>() };
			if (!CanCollide(entity, collider, e2, collider2)) {
				return;
			}
			if (Overlaps(GetTransform(entity), collider, GetTransform(e2), collider2)) {
				// ProcessCallback may invalidate all component references.
				ProcessCallback<T>(entity, e2, {});
			}
		};

		for (auto [e2, b2] : boxes) {
			process_overlap(b2, e2);
		}

		for (auto [e2, c2] : circles) {
			process_overlap(c2, e2);
		}
	}

	template <typename T>
	static void Intersect(
		ecs::Entity entity, const ecs::EntitiesWith<BoxCollider>& boxes,
		const ecs::EntitiesWith<CircleCollider>& circles
	) {
		if (entity.Get<T>().overlap_only) {
			return;
		}

		// CONSIDER: Is this criterion reasonable? Are there situations where we want an
		// intersection collision without a rigid body?
		if (!entity.Has<RigidBody>()) {
			return;
		}

		const auto process_intersection = [&](const auto& collider2, ecs::Entity e2) {
			const auto& collider{ entity.Get<T>() };
			if (collider2.overlap_only || !CanCollide(entity, collider, e2, collider2)) {
				return;
			}

			auto intersection{
				Intersects(GetTransform(entity), collider, GetTransform(e2), collider2)
			};

			if (!intersection.Occurred()) {
				return;
			}
			// ProcessCallback may invalidate all component references.
			if (!ProcessCallback<T>(entity, e2, intersection.normal)) {
				return;
			}
			auto& rb{ entity.Get<RigidBody>() };
			if (rb.immovable) {
				return;
			}
			if (entity.Has<Transform>()) {
				entity.Get<Transform>().position +=
					intersection.normal * (intersection.depth + slop);
			}
			rb.velocity = GetRemainingVelocity(
				rb.velocity, { 0.0f, intersection.normal }, entity.Get<T>().response
			);
		};

		for (auto [e2, b2] : boxes) {
			process_intersection(b2, e2);
		}

		for (auto [e2, c2] : circles) {
			process_intersection(c2, e2);
		}
	}

	// Updates the velocity of the object to prevent it from colliding with the target objects.
	template <typename T>
	static void Sweep(
		ecs::Entity entity, const ecs::EntitiesWith<BoxCollider>& boxes,
		const ecs::EntitiesWith<CircleCollider>&
			circles /* TODO: Fix or get rid of: , bool debug_draw = false */
	) {
		if (const auto& collider{ entity.Get<T>() };
			!collider.continuous || collider.overlap_only || !entity.Has<RigidBody, Transform>()) {
			return;
		}

		auto velocity{ entity.Get<RigidBody>().velocity * game.dt() };

		if (velocity.IsZero()) {
			return;
		}

		auto collisions{ GetSortedCollisions<T>(entity, boxes, circles, {}, velocity) };

		if (collisions.empty()) { // no collisions occured.
			// TODO: Fix or get rid of.
			/*if (debug_draw) {
				DrawDebugLine(transform.position, velocity, color::Gray);
			}*/
			return;
		}

		auto earliest{ collisions.front().c };

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

		AddEarliestCollisions(entity, collisions, entity.Get<T>().collisions);

		entity.Get<RigidBody>().velocity *= earliest.t;

		auto new_velocity{ GetRemainingVelocity(velocity, earliest, entity.Get<T>().response) };

		if (new_velocity.IsZero()) {
			return;
		}

		auto collisions2{
			GetSortedCollisions<T>(entity, boxes, circles, velocity * earliest.t, new_velocity)
		};

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

		auto earliest2{ collisions2.front().c };

		// TODO: Fix or get rid of.
		/*if (debug_draw) {
			DrawDebugLine(
				transform.position + velocity * earliest.t, new_velocity * earliest2.t, color::Green
			);
		}*/

		AddEarliestCollisions(entity, collisions2, entity.Get<T>().collisions);

		entity.Get<RigidBody>().AddImpulse(new_velocity / game.dt() * earliest2.t);
	}

private:
	friend class Game;
	friend class Physics;
	friend class ptgn::Scene;

	static void Update(ecs::Manager& manager);

	struct SweepCollision {
		SweepCollision() = default;

		SweepCollision(const RaycastResult& c, float dist2, ecs::Entity e) :
			e{ e }, c{ c }, dist2{ dist2 } {}

		// Collision entity.
		ecs::Entity e;
		RaycastResult c;
		float dist2{ 0.0f };
	};

	template <typename T>
	static bool ProcessCallback(ecs::Entity e1, ecs::Entity e2, const V2_float& normal) {
		// Process callback can invalidate the collider reference.
		if (e1.Get<T>().ProcessCallback(e1, e2)) {
			e1.Get<T>().collisions.emplace(e1, e2, normal);
			return true;
		}
		return false;
	}

	// T, S are the collider types.
	template <typename T, typename S>
	static void ProcessRaycast(
		std::vector<SweepCollision>& collisions, ecs::Entity entity, ecs::Entity e2,
		const V2_float& offset, const V2_float& vel
	) {
		auto& collider{ entity.Get<T>() };
		const auto& collider2{ e2.Get<S>() };
		if (collider2.overlap_only || !CanCollide(entity, collider, e2, collider2)) {
			return;
		}
		// TODO: Figure out a better way to do the second sweep without generating a new game
		// object or changing the position of the existing one.
		auto transform1{ GetTransform(entity) };
		auto transform2{ GetTransform(e2) };

		auto offset_transform{ transform1 };
		offset_transform.position += offset;

		auto raycast{
			Raycast(offset_transform, collider, GetRelativeVelocity(vel, e2), transform2, collider2)
		};

		// ProcessCallback may invalidate all component references.
		if (raycast.Occurred() && entity.Get<T>().ProcessCallback(entity, e2)) {
			auto center1{ GetCenter(transform1, collider) };
			auto center2{ GetCenter(transform2, collider2) };
			auto dist2{ (center1 - center2).MagnitudeSquared() };
			collisions.emplace_back(raycast, dist2, e2);
		}
	};

	// @param offset Offset from the transform position of the entity. This enables doing a second
	// sweep.
	// @param vel Velocity of the entity. As above, this enables a second sweep in the direction of
	// the remaining velocity.
	template <typename T>
	[[nodiscard]] static std::vector<SweepCollision> GetSortedCollisions(
		ecs::Entity entity, const ecs::EntitiesWith<BoxCollider>& boxes,
		const ecs::EntitiesWith<CircleCollider>& circles, const V2_float& offset,
		const V2_float& vel
	) {
		std::vector<SweepCollision> collisions;

		for (auto [e2, box2] : boxes) {
			ProcessRaycast<T, BoxCollider>(collisions, entity, e2, offset, vel);
		}

		for (auto [e2, circle2] : circles) {
			ProcessRaycast<T, CircleCollider>(collisions, entity, e2, offset, vel);
		}

		SortCollisions(collisions);

		return collisions;
	}

	template <typename T>
	static void HandleCollisions(
		ecs::Entity entity, const ecs::EntitiesWith<BoxCollider>& boxes,
		const ecs::EntitiesWith<CircleCollider>& circles
	) {
		auto& collider{ entity.Get<T>() };

		collider.ResetCollisions();

		if (!IsEnabled(entity)) {
			return;
		}

		Intersect<T>(entity, boxes, circles);
		Sweep<T>(entity, boxes, circles);
		Overlap<T>(entity, boxes, circles);

		collider = entity.Get<T>();

		for (const auto& prev : collider.prev_collisions) {
			PTGN_ASSERT(entity == prev.entity1);
			PTGN_ASSERT(entity != prev.entity2);
		}
		for (const auto& current : collider.collisions) {
			PTGN_ASSERT(entity == current.entity1);
			PTGN_ASSERT(entity != current.entity2);
		}

		collider.InvokeCollisionCallbacks();
	}

	// Adds all collisions which occurred at the earliest time to box.collisions. This ensures all
	// callbacks are called.
	static void AddEarliestCollisions(
		ecs::Entity entity, const std::vector<SweepCollision>& sweep_collisions,
		std::unordered_set<Collision>& entities
	);

	static void SortCollisions(std::vector<SweepCollision>& collisions);

	[[nodiscard]] static V2_float GetRemainingVelocity(
		const V2_float& velocity, const RaycastResult& c, CollisionResponse response
	);

	[[nodiscard]] static V2_float GetRelativeVelocity(const V2_float& vel, ecs::Entity e2);

	constexpr static float slop{ 0.0005f };
};

} // namespace impl

} // namespace ptgn