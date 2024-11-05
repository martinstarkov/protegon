#pragma once

#include <vector>

#include "components/collider.h"
#include "components/rigid_body.h"
#include "components/transform.h"
#include "ecs/ecs.h"
#include "math/raycast.h"
#include "math/vector2.h"

namespace ptgn::impl {

class Game;

class DynamicCollisionHandler {
public:
	DynamicCollisionHandler()										   = default;
	~DynamicCollisionHandler()										   = default;
	DynamicCollisionHandler(const DynamicCollisionHandler&)			   = delete;
	DynamicCollisionHandler(DynamicCollisionHandler&&)				   = default;
	DynamicCollisionHandler& operator=(const DynamicCollisionHandler&) = delete;
	DynamicCollisionHandler& operator=(DynamicCollisionHandler&&)	   = default;

	// @return Final velocity of the object to prevent them from colliding with the manager objects.
	V2_float Sweep(
		ecs::Entity entity, const RigidBody& rigid_body, const Transform& transform,
		BoxCollider& box, ecs::Manager& manager,
		CollisionResponse response = CollisionResponse::Slide, bool debug_draw = false
	);

private:
	struct SweepCollision {
		SweepCollision() = default;

		SweepCollision(const Raycast& c, float dist2, ecs::Entity e) :
			e{ e }, c{ c }, dist2{ dist2 } {}

		// Collision entity.
		ecs::Entity e;
		Raycast c;
		float dist2{ 0.0f };
	};

	static void SortCollisions(std::vector<SweepCollision>& collisions);

	[[nodiscard]] static V2_float GetRemainingVelocity(
		const V2_float& velocity, const Raycast& c, CollisionResponse response
	);
};

class CollisionHandler {
public:
	CollisionHandler()									 = default;
	~CollisionHandler()									 = default;
	CollisionHandler(const CollisionHandler&)			 = delete;
	CollisionHandler(CollisionHandler&&)				 = default;
	CollisionHandler& operator=(const CollisionHandler&) = delete;
	CollisionHandler& operator=(CollisionHandler&&)		 = default;

	DynamicCollisionHandler dynamic;

	void Update(ecs::Manager& manager);

private:
	friend class Game;

	void Init() {
		/* Possibly add stuff here in the future. */
	}

	void Shutdown();

	constexpr static float slop{ 0.005f };
};

} // namespace ptgn::impl