#pragma once

#include <vector>

#include "core/ecs/entity.h"
#include "math/raycast.h"
#include "math/vector2.h"
#include "physics/broadphase.h"
#include "physics/collider.h"

namespace ptgn {

class Scene;

namespace impl {

class Game;
class Physics;

class CollisionHandler {
public:
	CollisionHandler()										 = default;
	~CollisionHandler() noexcept							 = default;
	CollisionHandler(CollisionHandler&&) noexcept			 = default;
	CollisionHandler& operator=(CollisionHandler&&) noexcept = default;
	CollisionHandler& operator=(const CollisionHandler&)	 = delete;
	CollisionHandler(const CollisionHandler&)				 = delete;

	[[nodiscard]] static bool CanCollide(
		const Entity& entity1, const Collider& collider1, const Entity& entity2,
		const Collider& collider2
	);

private:
	friend class Game;
	friend class Physics;
	friend class ptgn::Scene;

	void Overlap(Entity& entity) const;

	void Intersect(Entity& entity, float dt);

	[[nodiscard]] static std::vector<Entity> GetSweepCandidates(
		Entity& entity1, const V2_float& velocity, const KDTree& tree
	);

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

	[[nodiscard]] std::vector<SweepCollision> GetSortedCollisions(
		Entity& entity1, const V2_float& offset, const V2_float& velocity1, float dt
	) const;

	// @param offset Offset from the transform position of the entity. This enables doing a second
	// sweep.
	// @param vel Velocity of the entity. As above, this enables a second sweep in the direction of
	// the remaining velocity.

	// Adds all collisions which occurred at the earliest time to box.collisions. This ensures all
	// callbacks are called.
	static void AddEarliestCollisions(
		Entity& entity, const std::vector<SweepCollision>& sweep_collisions
	);

	static void SortCollisions(std::vector<SweepCollision>& collisions);

	[[nodiscard]] static V2_float GetRemainingVelocity(
		const V2_float& velocity, const RaycastResult& collision, CollisionResponse response
	);

	[[nodiscard]] static V2_float GetRelativeVelocity(
		const V2_float& velocity1, const Entity& entity2, float dt
	);

	void UpdateKDTree(const Entity& entity, float dt);

	// Updates the velocity of the object to prevent it from colliding with the target objects.
	void Sweep(Entity& entity, float dt);

	void Update(Scene& scene);

	KDTree static_tree_{ 100 };
	KDTree dynamic_tree_{ 100 };

	constexpr static float slop_{ 0.0005f };
	constexpr static std::size_t max_sweep_iterations_{ 4 };
};

} // namespace impl

} // namespace ptgn