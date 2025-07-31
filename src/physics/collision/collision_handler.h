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
#include "serialization/json_archiver.h"

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
		const Entity& entity1, const T& colliderA, const Entity& entity2, const S& colliderB
	) {
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
		if (!colliderA.CanCollideWith(colliderB.GetCollisionCategory())) {
			return false;
		}
		return true;
	}

	template <typename T, typename S>
	[[nodiscard]] static bool Overlaps(
		Transform a, T A, std::optional<V2_float> rot_center_A, Transform b, S B,
		std::optional<V2_float> rot_center_B
	) {
		if constexpr (std::is_same_v<T, BoxCollider> && std::is_same_v<S, BoxCollider>) {
			A.size	   *= Abs(a.scale);
			a.position += GetOriginOffset(A.origin, A.size).Rotated(a.rotation);
			B.size	   *= Abs(b.scale);
			b.position += GetOriginOffset(B.origin, B.size).Rotated(b.rotation);
			return impl::OverlapRectRect(
				a.position, A.size, a.rotation, rot_center_A, b.position, B.size, b.rotation,
				rot_center_B
			);
		} else if constexpr (std::is_same_v<T, CircleCollider> &&
							 std::is_same_v<S, CircleCollider>) {
			A.radius *= Abs(a.scale.x);
			B.radius *= Abs(b.scale.x);
			return impl::OverlapCircleCircle(a.position, A.radius, b.position, B.radius);
		} else if constexpr (std::is_same_v<T, BoxCollider> && std::is_same_v<S, CircleCollider>) {
			B.radius   *= Abs(b.scale.x);
			A.size	   *= Abs(a.scale);
			a.position += GetOriginOffset(A.origin, A.size).Rotated(a.rotation);
			return impl::OverlapCircleRect(
				b.position, B.radius, a.position, A.size, a.rotation, rot_center_A
			);
		} else if constexpr (std::is_same_v<T, CircleCollider> && std::is_same_v<S, BoxCollider>) {
			A.radius   *= Abs(a.scale.x);
			B.size	   *= Abs(b.scale);
			b.position += GetOriginOffset(B.origin, B.size).Rotated(b.rotation);
			return impl::OverlapCircleRect(
				a.position, A.radius, b.position, B.size, b.rotation, rot_center_B
			);
		} else {
			static_assert(false, "Unrecognized collider types");
		}
	}

	template <typename T, typename S>
	static void ProcessOverlap(Entity entity, Entity entity2) {
		auto& collider{ entity.Get<T>() };
		auto& collider2{ entity2.Get<S>() };

		if (!CanCollide(entity, collider, entity2, collider2)) {
			return;
		}
		// ProcessCallback may invalidate all component references.

		if (!entity.Get<T>().ProcessCallback(entity, entity2) ||
			!entity2.Get<S>().ProcessCallback(entity2, entity)) {
			return;
		}

		auto rot_center_1{ entity.GetRotationCenter() };
		auto rot_center_2{ entity2.GetRotationCenter() };

		if (!Overlaps(
				entity.GetAbsoluteTransform(), entity.Get<T>(), rot_center_1,
				entity2.GetAbsoluteTransform(), entity2.Get<S>(), rot_center_2
			)) {
			return;
		}

		collider = entity.Get<T>();
		collider.collisions.emplace(entity2, V2_float{});

		collider2 = entity2.Get<S>();
		collider2.collisions.emplace(entity, V2_float{});
	}

	template <typename T>
	static void Overlap(
		Entity entity, const std::vector<Entity>& boxes, const std::vector<Entity>& circles
	) {
		// Optional: Make overlaps only work one way.
		if (!entity.Get<T>().overlap_only) {
			return;
		}

		for (const auto& entity2 : boxes) {
			ProcessOverlap<T, BoxCollider>(entity, entity2);
		}

		for (const auto& entity2 : circles) {
			ProcessOverlap<T, CircleCollider>(entity, entity2);
		}
	}

	template <typename T, typename S>
	[[nodiscard]] static Intersection Intersects(
		Transform a, T A, std::optional<V2_float> rot_center_A, Transform b, S B,
		std::optional<V2_float> rot_center_B
	) {
		if constexpr (std::is_same_v<T, BoxCollider> && std::is_same_v<S, BoxCollider>) {
			A.size	   *= Abs(a.scale);
			a.position += GetOriginOffset(A.origin, A.size).Rotated(a.rotation);
			B.size	   *= Abs(b.scale);
			b.position += GetOriginOffset(B.origin, B.size).Rotated(b.rotation);
			return impl::IntersectRectRect(
				a.position, A.size, a.rotation, rot_center_A, b.position, B.size, b.rotation,
				rot_center_B
			);
		} else if constexpr (std::is_same_v<T, CircleCollider> &&
							 std::is_same_v<S, CircleCollider>) {
			A.radius *= Abs(a.scale.x);
			B.radius *= Abs(b.scale.x);
			return impl::IntersectCircleCircle(a.position, A.radius, b.position, B.radius);
		} else if constexpr (std::is_same_v<T, BoxCollider> && std::is_same_v<S, CircleCollider>) {
			B.radius   *= Abs(b.scale.x);
			A.size	   *= Abs(a.scale);
			a.position += GetOriginOffset(A.origin, A.size).Rotated(a.rotation);
			return impl::IntersectCircleRect(
				b.position, B.radius, a.position, A.size, a.rotation, rot_center_A
			);
		} else if constexpr (std::is_same_v<T, CircleCollider> && std::is_same_v<S, BoxCollider>) {
			A.radius   *= Abs(a.scale.x);
			B.size	   *= Abs(b.scale);
			b.position += GetOriginOffset(B.origin, B.size).Rotated(b.rotation);
			return impl::IntersectCircleRect(
				a.position, A.radius, b.position, B.size, b.rotation, rot_center_B
			);
		} else {
			static_assert(false, "Unrecognized collider types");
		}
	}

	template <typename T, typename S>
	static void ProcessIntersection(Entity entity, Entity entity2) {
		const auto& collider{ entity.Get<T>() };
		const auto& collider2{ entity2.Get<S>() };

		if (collider2.overlap_only || !CanCollide(entity, collider, entity2, collider2)) {
			return;
		}

		// ProcessCallback may invalidate all component references.
		if (!entity.Get<T>().ProcessCallback(entity, entity2) ||
			!entity2.Get<S>().ProcessCallback(entity2, entity)) {
			return;
		}

		auto transform1{ entity.GetAbsoluteTransform() };
		auto transform2{ entity2.GetAbsoluteTransform() };
		auto rot_center_1{ entity.GetRotationCenter() };
		auto rot_center_2{ entity2.GetRotationCenter() };

		auto intersection{
			Intersects(transform1, collider, rot_center_1, transform2, collider2, rot_center_2)
		};

		if (!intersection.Occurred()) {
			return;
		}

		entity.Get<T>().collisions.emplace(entity2, intersection.normal);
		entity2.Get<S>().collisions.emplace(entity, -intersection.normal);

		if (!entity.Has<RigidBody>()) {
			return;
		}

		auto& rigid_body{ entity.Get<RigidBody>() };

		PhysicsBody body{ entity };

		if (body.IsImmovable()) {
			return;
		}
		/*if (entity.Has<Transform>()) {
			entity.GetPosition() +=
				intersection.normal * (intersection.depth + slop);
		}*/
		auto& root_transform{ body.GetRootTransform() };

		root_transform.position += intersection.normal * (intersection.depth + slop);

		rigid_body.velocity = GetRemainingVelocity(
			rigid_body.velocity, { 0.0f, intersection.normal }, entity.Get<T>().response
		);
	}

	template <typename T>
	static void Intersect(
		Entity entity, const std::vector<Entity>& boxes, const std::vector<Entity>& circles
	) {
		if (entity.Get<T>().overlap_only) {
			return;
		}

		for (const auto& entity2 : boxes) {
			ProcessIntersection<T, BoxCollider>(entity, entity2);
		}

		for (const auto& entity2 : circles) {
			ProcessIntersection<T, CircleCollider>(entity, entity2);
		}
	}

	// Updates the velocity of the object to prevent it from colliding with the target objects.
	template <typename T>
	static void Sweep(
		Entity entity, const std::vector<Entity>& boxes,
		const std::vector<Entity>& circles /* TODO: Fix or get rid of: , bool debug_draw = false */
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

		auto earliest2{ collisions2.front().collision };

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

	static void Update(Scene& scene);

	struct SweepCollision {
		SweepCollision() = default;

		SweepCollision(
			const RaycastResult& raycast_result, float distance_squared, Entity sweep_entity
		) :
			entity{ sweep_entity }, collision{ raycast_result }, dist2{ distance_squared } {}

		// Collision entity.
		Entity entity;
		RaycastResult collision;
		float dist2{ 0.0f };
	};

	template <typename T, typename S>
	[[nodiscard]] static RaycastResult Raycast(
		Transform a, T A, const V2_float& ray, Transform b, S B
	) {
		if constexpr (std::is_same_v<T, BoxCollider> && std::is_same_v<S, BoxCollider>) {
			A.size	   *= Abs(a.scale);
			a.position += GetOriginOffset(A.origin, A.size).Rotated(a.rotation);
			B.size	   *= Abs(b.scale);
			b.position += GetOriginOffset(B.origin, B.size).Rotated(b.rotation);
			return impl::RaycastRectRect(a.position, A.size, ray, b.position, B.size);
		} else if constexpr (std::is_same_v<T, CircleCollider> &&
							 std::is_same_v<S, CircleCollider>) {
			A.radius *= Abs(a.scale.x);
			B.radius *= Abs(b.scale.x);
			return impl::RaycastCircleCircle(a.position, A.radius, ray, b.position, B.radius);
		} else if constexpr (std::is_same_v<T, BoxCollider> && std::is_same_v<S, CircleCollider>) {
			A.size	   *= Abs(a.scale);
			a.position += GetOriginOffset(A.origin, A.size).Rotated(a.rotation);
			B.radius   *= Abs(b.scale.x);
			return impl::RaycastRectCircle(a.position, A.size, ray, b.position, B.radius);
		} else if constexpr (std::is_same_v<T, CircleCollider> && std::is_same_v<S, BoxCollider>) {
			A.radius   *= Abs(a.scale.x);
			B.size	   *= Abs(b.scale);
			b.position += GetOriginOffset(B.origin, B.size).Rotated(b.rotation);
			return impl::RaycastCircleRect(a.position, A.radius, ray, b.position, B.size);
		} else {
			static_assert(false, "Unrecognized collider types");
		}
	}

	template <typename T>
	[[nodiscard]] static V2_float GetCenter(const Transform& transform, const T& collider) {
		if constexpr (std::is_same_v<T, BoxCollider>) {
			return impl::GetCenter(transform, collider.size, collider.origin);
		} else if constexpr (std::is_same_v<T, CircleCollider>) {
			return transform.position;
		} else {
			static_assert(false, "Unrecognized collider types");
		}
	}

	// T, S are the collider types.
	template <typename T, typename S>
	static void ProcessRaycast(
		std::vector<SweepCollision>& collisions, Entity entity, Entity entity2,
		const V2_float& offset, const V2_float& vel
	) {
		auto& collider{ entity.Get<T>() };
		const auto& collider2{ entity2.Get<S>() };

		if (collider2.overlap_only || !CanCollide(entity, collider, entity2, collider2)) {
			return;
		}

		// ProcessCallback may invalidate all component references.
		if (!entity.Get<T>().ProcessCallback(entity, entity2) ||
			!entity2.Get<S>().ProcessCallback(entity2, entity)) {
			return;
		}

		auto transform1{ entity.GetAbsoluteTransform() };
		auto transform2{ entity2.GetAbsoluteTransform() };

		auto offset_transform{ transform1 };
		offset_transform.position += offset;

		auto raycast{ Raycast(
			offset_transform, entity.Get<T>(), GetRelativeVelocity(vel, entity2), transform2,
			entity2.Get<S>()
		) };

		if (raycast.Occurred()) {
			auto center1{ GetCenter(offset_transform, entity.Get<T>()) };
			auto center2{ GetCenter(transform2, entity2.Get<S>()) };
			auto dist2{ (center1 - center2).MagnitudeSquared() };
			collisions.emplace_back(raycast, dist2, entity2);
		}
	};

	// @param offset Offset from the transform position of the entity. This enables doing a second
	// sweep.
	// @param vel Velocity of the entity. As above, this enables a second sweep in the direction of
	// the remaining velocity.
	template <typename T>
	[[nodiscard]] static std::vector<SweepCollision> GetSortedCollisions(
		Entity entity, const std::vector<Entity>& boxes, const std::vector<Entity>& circles,
		const V2_float& offset, const V2_float& vel
	) {
		std::vector<SweepCollision> collisions;

		for (const auto& entity2 : boxes) {
			ProcessRaycast<T, BoxCollider>(collisions, entity, entity2, offset, vel);
		}

		for (const auto& entity2 : circles) {
			ProcessRaycast<T, CircleCollider>(collisions, entity, entity2, offset, vel);
		}

		SortCollisions(collisions);

		return collisions;
	}

	template <typename T>
	static void HandleCollisions(
		Entity entity, const std::vector<Entity>& boxes, const std::vector<Entity>& circles
	) {
		auto& collider{ entity.Get<T>() };

		PTGN_ASSERT(entity.IsEnabled());

		Intersect<T>(entity, boxes, circles);
		Sweep<T>(entity, boxes, circles);
		Overlap<T>(entity, boxes, circles);

		collider = entity.Get<T>();

		for (const auto& prev : collider.prev_collisions) {
			PTGN_ASSERT(entity != prev.entity);
		}
		for (const auto& current : collider.collisions) {
			PTGN_ASSERT(entity != current.entity);
		}

		collider.InvokeCollisionCallbacks(entity);
	}

	// Adds all collisions which occurred at the earliest time to box.collisions. This ensures all
	// callbacks are called.
	static void AddEarliestCollisions(
		Entity entity, const std::vector<SweepCollision>& sweep_collisions,
		std::unordered_set<Collision>& entities
	);

	static void SortCollisions(std::vector<SweepCollision>& collisions);

	[[nodiscard]] static V2_float GetRemainingVelocity(
		const V2_float& velocity, const RaycastResult& collision, CollisionResponse response
	);

	[[nodiscard]] static V2_float GetRelativeVelocity(const V2_float& velocity, Entity entity2);

	constexpr static float slop{ 0.0005f };
};

} // namespace impl

} // namespace ptgn