#pragma once

#include <ostream>
#include <vector>

#include "core/event/event.h"
#include "core/math/raycast.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/physics/broadphase.h"
#include "runtime/physics/collider.h"

namespace ptgn {

class Scene;

struct CollisionEvent : public Event<CollisionEvent> {
	Collision collision;
};

struct OverlapStart : public Event<OverlapStart> {
	Entity overlap_entity;
};

struct OverlapContinue : public Event<OverlapContinue> {
	Entity overlap_entity;
};

struct OverlapStop : public Event<OverlapStop> {
	Entity overlap_entity;
};

namespace impl {

class Physics;

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

class CollisionHandler {
public:
	CollisionHandler()										 = default;
	~CollisionHandler() noexcept							 = default;
	CollisionHandler(CollisionHandler&&) noexcept			 = default;
	CollisionHandler& operator=(CollisionHandler&&) noexcept = default;
	CollisionHandler& operator=(const CollisionHandler&)	 = delete;
	CollisionHandler(const CollisionHandler&)				 = delete;

	[[nodiscard]] static bool CanCollide(
		Entity entity1, const Collider& collider1, Entity entity2, const Collider& collider2
	);

private:
	friend class Physics;
	friend class ptgn::Scene;

	void Overlap(Entity entity) const;

	void Intersect(Entity entity, float dt);

	[[nodiscard]] static std::vector<Entity> GetSweepCandidates(
		Entity entity1, V2_float velocity, const KDTree& tree
	);

	[[nodiscard]] std::vector<SweepCollision> GetSortedCollisions(
		Entity entity1, V2_float offset, V2_float velocity1, float dt
	) const;

	// @param offset Offset from the transform position of the entity. This enables doing a
	// second sweep.
	// @param vel Velocity of the entity. As above, this enables a second sweep in the direction
	// of the remaining velocity.

	// Adds all collisions which occurred at the earliest time to box.collisions. This ensures
	// all callbacks are called.
	static void AddEarliestCollisions(
		Entity entity, const std::vector<SweepCollision>& sweep_collisions
	);

	static void SortCollisions(std::vector<SweepCollision>& collisions);

	[[nodiscard]] static V2_float GetRemainingVelocity(
		V2_float velocity, const RaycastResult& collision, CollisionResponse response
	);

	[[nodiscard]] static V2_float GetRelativeVelocity(V2_float velocity1, Entity entity2, float dt);

	void UpdateKDTree(Entity entity, float dt);

	// Updates the velocity of the object to prevent it from colliding with the target objects.
	void Sweep(Entity entity, float dt);

	void Update(Scene& scene);

	KDTree static_tree_{ 100 };
	KDTree dynamic_tree_{ 100 };

	constexpr static float slop_{ 0.0005f };
	constexpr static std::size_t max_sweep_iterations_{ 4 };
};

} // namespace impl

std::ostream& operator<<(std::ostream& os, const impl::SweepCollision& sweep_collision);

} // namespace ptgn