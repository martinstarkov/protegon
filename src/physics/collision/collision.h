#pragma once

#include <type_traits>
#include <unordered_set>
#include <vector>

#include "common/assert.h"
#include "components/common.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "math/geometry.h"
#include "math/vector2.h"
#include "physics/collision/collider.h"
#include "physics/collision/intersect.h"
#include "physics/collision/overlap.h"
#include "physics/collision/raycast.h"
#include "physics/rigid_body.h"

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
		const Entity& e1, const T& colliderA, const Entity& e2, const S& colliderB
	) {
		PTGN_ASSERT(e1.IsEnabled() && e2.IsEnabled());
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

	template <typename T, typename S>
	[[nodiscard]] static bool Overlaps(Transform a, T A, Transform b, S B) {
		if constexpr (std::is_same_v<T, BoxCollider> && std::is_same_v<S, BoxCollider>) {
			A.size	   *= a.scale;
			a.position += GetOriginOffset(A.origin, A.size);
			B.size	   *= b.scale;
			b.position += GetOriginOffset(B.origin, B.size);
			return impl::OverlapRectRect(
				a.position, A.size, a.rotation, b.position, B.size, b.rotation
			);
		} else if constexpr (std::is_same_v<T, CircleCollider> && std::is_same_v<S, CircleCollider>) {
			A.radius *= a.scale.x;
			B.radius *= b.scale.x;
			return impl::OverlapCircleCircle(a.position, A.radius, b.position, B.radius);
		} else if constexpr (std::is_same_v<T, BoxCollider> && std::is_same_v<S, CircleCollider>) {
			B.radius   *= b.scale.x;
			A.size	   *= a.scale;
			a.position += GetOriginOffset(A.origin, A.size);
			return impl::OverlapCircleRect(b.position, B.radius, a.position, A.size);
		} else if constexpr (std::is_same_v<T, CircleCollider> && std::is_same_v<S, BoxCollider>) {
			A.radius   *= a.scale.x;
			B.size	   *= b.scale;
			b.position += GetOriginOffset(B.origin, B.size);
			return impl::OverlapCircleRect(a.position, A.radius, b.position, B.size);
		} else {
			static_assert("Unrecognized collider types");
		}
	}

	template <typename T>
	static void Overlap(
		Entity entity, const EntitiesWith<true, Enabled, BoxCollider>& boxes,
		const EntitiesWith<true, Enabled, CircleCollider>& circles
	) {
		if (!entity.Get<T>().overlap_only) {
			return;
		}

		const auto process_overlap = [&](const auto& collider2, Entity e2) {
			const auto& collider{ entity.Get<T>() };
			if (!CanCollide(entity, collider, e2, collider2)) {
				return;
			}
			if (Overlaps(entity.GetTransform(), collider, e2.GetTransform(), collider2)) {
				// ProcessCallback may invalidate all component references.
				ProcessCallback<T>(entity, e2, {});
			}
		};

		for (auto [e2, enabled, b2] : boxes) {
			if (!enabled) {
				continue;
			}
			process_overlap(b2, e2);
		}

		for (auto [e2, enabled, c2] : circles) {
			if (!enabled) {
				continue;
			}
			process_overlap(c2, e2);
		}
	}

	template <typename T, typename S>
	[[nodiscard]] static Intersection Intersects(Transform a, T A, Transform b, S B) {
		if constexpr (std::is_same_v<T, BoxCollider> && std::is_same_v<S, BoxCollider>) {
			A.size	   *= a.scale;
			a.position += GetOriginOffset(A.origin, A.size);
			B.size	   *= b.scale;
			b.position += GetOriginOffset(B.origin, B.size);
			return impl::IntersectRectRect(
				a.position, A.size, a.rotation, b.position, B.size, b.rotation
			);
		} else if constexpr (std::is_same_v<T, CircleCollider> && std::is_same_v<S, CircleCollider>) {
			A.radius *= a.scale.x;
			B.radius *= b.scale.x;
			return impl::IntersectCircleCircle(a.position, A.radius, b.position, B.radius);
		} else if constexpr (std::is_same_v<T, BoxCollider> && std::is_same_v<S, CircleCollider>) {
			B.radius   *= b.scale.x;
			A.size	   *= a.scale;
			a.position += GetOriginOffset(A.origin, A.size);
			return impl::IntersectCircleRect(b.position, B.radius, a.position, A.size);
		} else if constexpr (std::is_same_v<T, CircleCollider> && std::is_same_v<S, BoxCollider>) {
			A.radius   *= a.scale.x;
			B.size	   *= b.scale;
			b.position += GetOriginOffset(B.origin, B.size);
			return impl::IntersectCircleRect(a.position, A.radius, b.position, B.size);
		} else {
			static_assert("Unrecognized collider types");
		}
	}

	template <typename T>
	static void Intersect(
		Entity entity, const EntitiesWith<true, Enabled, BoxCollider>& boxes,
		const EntitiesWith<true, Enabled, CircleCollider>& circles
	) {
		if (entity.Get<T>().overlap_only) {
			return;
		}

		// CONSIDER: Is this criterion reasonable? Are there situations where we want an
		// intersection collision without a rigid body?
		if (!entity.Has<RigidBody>()) {
			return;
		}

		const auto process_intersection = [&](const auto& collider2, Entity e2) {
			const auto& collider{ entity.Get<T>() };
			if (collider2.overlap_only || !CanCollide(entity, collider, e2, collider2)) {
				return;
			}

			auto intersection{
				Intersects(entity.GetTransform(), collider, e2.GetTransform(), collider2)
			};

			if (!intersection.Occurred()) {
				return;
			}
			// ProcessCallback may invalidate all component references.
			if (!ProcessCallback<T>(entity, e2, intersection.normal)) {
				return;
			}
			auto& rb{ entity.Get<RigidBody>() };
			if (entity.IsImmovable()) {
				return;
			}
			/*if (entity.Has<Transform>()) {
				entity.Get<Transform>().position +=
					intersection.normal * (intersection.depth + slop);
			}*/
			auto& root_transform{ entity.GetRootTransform() };
			root_transform.position += intersection.normal * (intersection.depth + slop);
			rb.velocity				 = GetRemainingVelocity(
				 rb.velocity, { 0.0f, intersection.normal }, entity.Get<T>().response
			 );
		};

		for (auto [e2, enabled, b2] : boxes) {
			if (!enabled) {
				continue;
			}
			process_intersection(b2, e2);
		}

		for (auto [e2, enabled, c2] : circles) {
			if (!enabled) {
				continue;
			}
			process_intersection(c2, e2);
		}
	}

	// Updates the velocity of the object to prevent it from colliding with the target objects.
	template <typename T>
	static void Sweep(
		Entity entity, const EntitiesWith<true, Enabled, BoxCollider>& boxes,
		const EntitiesWith<true, Enabled, CircleCollider>&
			circles /* TODO: Fix or get rid of: , bool debug_draw = false */
	) {
		if (const auto& collider{ entity.Get<T>() };
			!collider.continuous || collider.overlap_only || !entity.Has<RigidBody>()) {
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

	static void Update(Manager& manager);

	struct SweepCollision {
		SweepCollision() = default;

		SweepCollision(const RaycastResult& c, float dist2, Entity e) :
			e{ e }, c{ c }, dist2{ dist2 } {}

		// Collision entity.
		Entity e;
		RaycastResult c;
		float dist2{ 0.0f };
	};

	template <typename T>
	static bool ProcessCallback(Entity e1, Entity e2, const V2_float& normal) {
		// Process callback can invalidate the collider reference.
		if (e1.Get<T>().ProcessCallback(e1, e2)) {
			e1.Get<T>().collisions.emplace(e1, e2, normal);
			return true;
		}
		return false;
	}

	template <typename T, typename S>
	[[nodiscard]] static RaycastResult Raycast(
		Transform a, T A, const V2_float& ray, Transform b, S B
	) {
		if constexpr (std::is_same_v<T, BoxCollider> && std::is_same_v<S, BoxCollider>) {
			A.size	   *= a.scale;
			a.position += GetOriginOffset(A.origin, A.size);
			B.size	   *= b.scale;
			b.position += GetOriginOffset(B.origin, B.size);
			return impl::RaycastRectRect(a.position, A.size, ray, b.position, B.size);
		} else if constexpr (std::is_same_v<T, CircleCollider> && std::is_same_v<S, CircleCollider>) {
			A.radius *= a.scale.x;
			B.radius *= b.scale.x;
			return impl::RaycastCircleCircle(a.position, A.radius, ray, b.position, B.radius);
		} else if constexpr (std::is_same_v<T, BoxCollider> && std::is_same_v<S, CircleCollider>) {
			A.size	   *= a.scale;
			a.position += GetOriginOffset(A.origin, A.size);
			B.radius   *= b.scale.x;
			return impl::RaycastRectCircle(a.position, A.size, ray, b.position, B.radius);
		} else if constexpr (std::is_same_v<T, CircleCollider> && std::is_same_v<S, BoxCollider>) {
			A.radius   *= a.scale.x;
			B.size	   *= b.scale;
			b.position += GetOriginOffset(B.origin, B.size);
			return impl::RaycastCircleRect(a.position, A.radius, ray, b.position, B.size);
		} else {
			static_assert("Unrecognized collider types");
		}
	}

	template <typename T>
	[[nodiscard]] static V2_float GetCenter(const Transform& transform, const T& collider) {
		if constexpr (std::is_same_v<T, BoxCollider>) {
			return impl::GetCenter(transform, collider.size, collider.origin);
		} else if constexpr (std::is_same_v<T, CircleCollider>) {
			return transform.position;
		} else {
			static_assert("Unrecognized collider types");
		}
	}

	// T, S are the collider types.
	template <typename T, typename S>
	static void ProcessRaycast(
		std::vector<SweepCollision>& collisions, Entity entity, Entity e2, const V2_float& offset,
		const V2_float& vel
	) {
		auto& collider{ entity.Get<T>() };
		const auto& collider2{ e2.Get<S>() };
		if (collider2.overlap_only || !CanCollide(entity, collider, e2, collider2)) {
			return;
		}
		// TODO: Figure out a better way to do the second sweep without generating a new game
		// object or changing the position of the existing one.
		auto transform1{ entity.GetTransform() };
		auto transform2{ e2.GetTransform() };

		auto offset_transform{ transform1 };
		offset_transform.position += offset;

		auto raycast{
			Raycast(offset_transform, collider, GetRelativeVelocity(vel, e2), transform2, collider2)
		};

		// ProcessCallback may invalidate all component references.
		if (raycast.Occurred() && entity.Get<T>().ProcessCallback(entity, e2)) {
			// TODO: Fix these GetCenter functions.
			auto center1{ GetCenter(offset_transform, collider) };
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
		Entity entity, const EntitiesWith<true, Enabled, BoxCollider>& boxes,
		const EntitiesWith<true, Enabled, CircleCollider>& circles, const V2_float& offset,
		const V2_float& vel
	) {
		std::vector<SweepCollision> collisions;

		for (auto [e2, enabled, box2] : boxes) {
			if (!enabled) {
				continue;
			}
			ProcessRaycast<T, BoxCollider>(collisions, entity, e2, offset, vel);
		}

		for (auto [e2, enabled, circle2] : circles) {
			if (!enabled) {
				continue;
			}
			ProcessRaycast<T, CircleCollider>(collisions, entity, e2, offset, vel);
		}

		SortCollisions(collisions);

		return collisions;
	}

	template <typename T>
	static void HandleCollisions(
		Entity entity, const EntitiesWith<true, Enabled, BoxCollider>& boxes,
		const EntitiesWith<true, Enabled, CircleCollider>& circles
	) {
		auto& collider{ entity.Get<T>() };

		collider.ResetCollisions();

		PTGN_ASSERT(entity.IsEnabled());

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
		Entity entity, const std::vector<SweepCollision>& sweep_collisions,
		std::unordered_set<Collision>& entities
	);

	static void SortCollisions(std::vector<SweepCollision>& collisions);

	[[nodiscard]] static V2_float GetRemainingVelocity(
		const V2_float& velocity, const RaycastResult& c, CollisionResponse response
	);

	[[nodiscard]] static V2_float GetRelativeVelocity(const V2_float& vel, Entity e2);

	constexpr static float slop{ 0.0005f };
};

} // namespace impl

} // namespace ptgn