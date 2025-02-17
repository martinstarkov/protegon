#pragma once

#include <type_traits>
#include <unordered_set>
#include <vector>

#include "components/transform.h"
#include "core/game.h"
#include "ecs/ecs.h"
#include "math/collider.h"
#include "math/geometry/circle.h"
#include "math/geometry/intersection.h"
#include "math/geometry/polygon.h"
#include "math/raycast.h"
#include "math/vector2.h"
#include "physics/rigid_body.h"
#include "renderer/color.h"
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
			if (!collider.CanCollideWith(collider2)) {
				return;
			}
			auto s1{ GetShape(collider) };
			auto s2{ GetShape(collider2) };
			if (s1.Overlaps(s2)) {
				// ProcessCallback may invalidate all component references.
				ProcessCallback<T>(entity, collider2.GetParent(e2), {});
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

		auto& rb{ entity.Get<RigidBody>() };

		if (rb.immovable) {
			return;
		}

		const auto process_intersection = [&](const auto& collider2, ecs::Entity e2) {
			const auto& collider{ entity.Get<T>() };
			if (collider2.overlap_only || !collider.CanCollideWith(collider2)) {
				return;
			}
			auto s1{ GetShape(collider) };
			auto s2{ GetShape(collider2) };
			Intersection c{ s1.Intersects(s2) };
			if (!c.Occurred()) {
				return;
			}
			// ProcessCallback may invalidate all component references.
			if (!ProcessCallback<T>(entity, collider2.GetParent(e2), c.normal)) {
				return;
			}
			if (entity.Has<Transform>()) {
				entity.Get<Transform>().position += c.normal * (c.depth + slop);
			}
			rb.velocity = GetRemainingVelocity(
				rb.velocity, Raycast{ 0.0f, c.normal }, entity.Get<T>().response
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

		// Transform transform{ entity.Get<Transform>() };

		auto velocity{ entity.Get<RigidBody>().velocity * game.dt() };

		if (velocity.IsZero()) {
			return;
		}

		auto collisions{ GetSortedCollisions<T>(entity, boxes, circles, {}, velocity) };

		if (collisions.empty()) { // no collisions occured.
			// TODO: Fix or get rid of.
			/*if (debug_draw) {
				DrawVelocity(transform.position, velocity, color::Gray);
			}*/
			return;
		}

		auto earliest{ collisions.front().c };

		// TODO: Fix or get rid of.
		/*if (debug_draw) {
			DrawVelocity(transform.position, velocity * earliest.t, color::Blue);
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
				DrawVelocity(
					transform.position + velocity * earliest.t, new_velocity, color::Orange
				);
			}*/

			entity.Get<RigidBody>().AddImpulse(new_velocity / game.dt());
			return;
		}

		auto earliest2{ collisions2.front().c };

		// TODO: Fix or get rid of.
		/*if (debug_draw) {
			DrawVelocity(
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

	// TODO: Fix or get rid of.
	/*static void DrawVelocity(const V2_float& start, const V2_float& vel, const Color& color);*/

	struct SweepCollision {
		SweepCollision() = default;

		SweepCollision(const Raycast& c, float dist2, ecs::Entity e) :
			e{ e }, c{ c }, dist2{ dist2 } {}

		// Collision entity.
		ecs::Entity e;
		Raycast c;
		float dist2{ 0.0f };
	};

	template <typename T>
	[[nodiscard]] static auto GetShape(const T& collider) {
		if constexpr (std::is_same_v<T, BoxCollider>) {
			return collider.GetAbsoluteRect();
		} else if constexpr (std::is_same_v<T, CircleCollider>) {
			return collider.GetAbsoluteCircle();
		} else {
			PTGN_ERROR("Failed to identify collider shape");
		}
	}

	template <typename T>
	static bool ProcessCallback(
		ecs::Entity e1_parent, ecs::Entity e2_parent, const V2_float& normal
	) {
		// Process callback can invalidate the collider reference.
		if (e1_parent.Get<T>().ProcessCallback(e1_parent, e2_parent)) {
			e1_parent.Get<T>().collisions.emplace(e1_parent, e2_parent, normal);
			return true;
		}
		return false;
	}

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

		const auto process_raycast = [&](const auto& collider2, ecs::Entity e2) {
			const auto& collider{ entity.Get<T>() };
			if (collider2.overlap_only || !collider.CanCollideWith(collider2)) {
				return;
			}
			auto s1{ GetShape(collider) };
			s1.Offset(offset);
			e2 = collider2.GetParent(e2);
			auto s2{ GetShape(collider2) };
			Raycast c{ s1.Raycast(GetRelativeVelocity(vel, e2), s2) };
			// ProcessCallback may invalidate all component references.
			if (c.Occurred() && entity.Get<T>().ProcessCallback(entity, e2)) {
				float dist2{ (s1.Center() - s2.Center()).MagnitudeSquared() };
				collisions.emplace_back(c, dist2, e2);
			}
		};

		for (auto [e2, box2] : boxes) {
			process_raycast(box2, e2);
		}

		for (auto [e2, circle2] : circles) {
			process_raycast(circle2, e2);
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

		if (!collider.enabled) {
			return;
		}

		ecs::Entity e{ collider.GetParent(entity) };

		Intersect<T>(e, boxes, circles);
		Sweep<T>(e, boxes, circles);
		Overlap<T>(e, boxes, circles);

		collider = entity.Get<T>();

		for (const auto& prev : collider.prev_collisions) {
			PTGN_ASSERT(e == prev.entity1);
			PTGN_ASSERT(e != prev.entity2);
		}
		for (const auto& current : collider.collisions) {
			PTGN_ASSERT(e == current.entity1);
			PTGN_ASSERT(e != current.entity2);
		}

		collider.InvokeCollisionCallbacks();
	}

	// Adds all collisions which occurred at the earliest time to box.collisions. This ensures all
	// callbacks are called.
	static void AddEarliestCollisions(
		ecs::Entity e, const std::vector<SweepCollision>& sweep_collisions,
		std::unordered_set<Collision>& entities
	);

	static void SortCollisions(std::vector<SweepCollision>& collisions);

	[[nodiscard]] static V2_float GetRemainingVelocity(
		const V2_float& velocity, const Raycast& c, CollisionResponse response
	);

	[[nodiscard]] static V2_float GetRelativeVelocity(const V2_float& vel, ecs::Entity e2);

	constexpr static float slop{ 0.0005f };
};

} // namespace impl

} // namespace ptgn