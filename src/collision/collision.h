#pragma once

#include <vector>

#include "collision/collider.h"
#include "collision/raycast.h"
#include "components/transform.h"
#include "ecs/ecs.h"
#include "math/vector2.h"
#include "physics/rigid_body.h"

namespace ptgn::impl {

class Game;

class CollisionHandler {
public:
	CollisionHandler()									 = default;
	~CollisionHandler()									 = default;
	CollisionHandler(const CollisionHandler&)			 = delete;
	CollisionHandler(CollisionHandler&&)				 = default;
	CollisionHandler& operator=(const CollisionHandler&) = delete;
	CollisionHandler& operator=(CollisionHandler&&)		 = default;

	void Update(ecs::Manager& manager) const;

	// Updates the velocity of the object to prevent it from colliding with the target objects.
	void Sweep(
		ecs::Entity entity, BoxCollider& box, const ecs::EntitiesWith<BoxCollider>& targets,
		bool debug_draw = false
	) const;

private:
	friend class Game;

	struct SweepCollision {
		SweepCollision() = default;

		SweepCollision(const Raycast& c, float dist2, ecs::Entity e) :
			e{ e }, c{ c }, dist2{ dist2 } {}

		// Collision entity.
		ecs::Entity e;
		Raycast c;
		float dist2{ 0.0f };
	};

	static void Overlap(
		ecs::Entity entity, BoxCollider& box, const ecs::EntitiesWith<BoxCollider>& targets
	);

	static void Intersect(
		ecs::Entity entity, BoxCollider& box, const ecs::EntitiesWith<BoxCollider>& targets
	);

	static void ProcessCallback(
		BoxCollider& b1, ecs::Entity e1_parent, ecs::Entity e2_parent, const V2_float& normal
	);

	// @param offset Offset from the transform position of the entity. This enables doing a second
	// sweep.
	// @param vel Velocity of the entity. As above, this enables a second sweep in the direction of
	// the remaining velocity.
	[[nodiscard]] static std::vector<SweepCollision> GetSortedCollisions(
		ecs::Entity entity, const BoxCollider& box, const ecs::EntitiesWith<BoxCollider>& targets,
		const V2_float& offset, const V2_float& vel
	);

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

	constexpr static float slop{ 0.005f };
};

} // namespace ptgn::impl