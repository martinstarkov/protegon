#pragma once

#include <unordered_set>
#include <vector>

#include "core/entity.h"
#include "math/raycast.h"
#include "math/vector2.h"
#include "physics/collision/collider.h"

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

	[[nodiscard]] static bool CanCollide(const Entity& entity1, const Entity& entity2);

	static void Overlap(Entity entity, const std::vector<Entity>& entities);

	static void Intersect(Entity entity, const std::vector<Entity>& entities);

	// Updates the velocity of the object to prevent it from colliding with the target objects.
	static void Sweep(
		Entity entity,
		const std::vector<Entity>& entities /* TODO: Fix or get rid of: , bool debug_draw = false */
	);

private:
	friend class Game;
	friend class Physics;
	friend class ptgn::Scene;

	static void Update(Scene& scene);

	struct SweepCollision {
		SweepCollision() = default;

		SweepCollision(
			const RaycastResult& raycast_result, float distance_squared, Entity sweep_entity
		);

		// Collision entity.
		Entity entity;
		RaycastResult collision;
		float dist2{ 0.0f };
	};

	// @param offset Offset from the transform position of the entity. This enables doing a second
	// sweep.
	// @param vel Velocity of the entity. As above, this enables a second sweep in the direction of
	// the remaining velocity.
	[[nodiscard]] static std::vector<SweepCollision> GetSortedCollisions(
		Entity entity, const std::vector<Entity>& entities, const V2_float& offset,
		const V2_float& vel
	);

	static void HandleCollisions(Entity entity, const std::vector<Entity>& entities);

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