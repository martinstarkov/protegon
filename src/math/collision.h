#pragma once

#include <vector>

#include "components/collider.h"
#include "components/rigid_body.h"
#include "components/transform.h"
#include "ecs/ecs.h"
#include "math/geometry/circle.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"

namespace ptgn {

struct DynamicCollision {
	float t{ 1.0f }; // time of impact.
	V2_float normal; // normal of impact (normalised).
};

namespace impl {

class DynamicCollisionHandler {
public:
	DynamicCollisionHandler()										   = default;
	~DynamicCollisionHandler()										   = default;
	DynamicCollisionHandler(const DynamicCollisionHandler&)			   = delete;
	DynamicCollisionHandler(DynamicCollisionHandler&&)				   = default;
	DynamicCollisionHandler& operator=(const DynamicCollisionHandler&) = delete;
	DynamicCollisionHandler& operator=(DynamicCollisionHandler&&)	   = default;

	// TODO: Move dynamic collision tests into the geometry classes.

	static bool LineLine(const Line& a, const Line& b, DynamicCollision& c);

	static bool LineCircle(const Line& a, const Circle& b, DynamicCollision& c);

	static bool LineRect(const Line& a, Rect b, DynamicCollision& c);

	static bool LineCapsule(const Line& a, const Capsule& b, DynamicCollision& c);

	// Velocity is taken relative to a, b is seen as static.
	static bool CircleCircle(
		const Circle& a, const V2_float& vel, const Circle& b, DynamicCollision& c
	);

	// Velocity is taken relative to a, b is seen as static.
	static bool CircleRect(
		const Circle& a, const V2_float& vel, const Rect& b, DynamicCollision& c
	);

	// Velocity is taken relative to a, b is seen as static.
	static bool RectRect(const Rect& a, const V2_float& vel, const Rect& b, DynamicCollision& c);

	// @return Updates the velocity of the object to prevent them from colliding with the other
	// objects in the manager.
	// @return Final velocity of the object to prevent them from colliding with the manager objects.
	V2_float Sweep(
		const std::vector<ecs::Entity>& excluded_entities, const RigidBody& rigid_body,
		const Transform& transform, BoxCollider& box, ecs::Manager& manager,
		CollisionResponse response = CollisionResponse::Slide, bool debug_draw = false
	);

private:
	struct SweepCollision {
		SweepCollision() = default;

		SweepCollision(const DynamicCollision& c, float dist2, ecs::Entity e) :
			e{ e }, c{ c }, dist2{ dist2 } {}

		// Collision entity.
		ecs::Entity e;
		DynamicCollision c;
		float dist2{ 0.0f };
	};

	[[nodiscard]] static DynamicCollision GetEarliestCollision(
		ecs::Entity e, const Rect& rect1, const V2_float& vel1, ecs::Manager& manager
	);

	static void SortCollisions(std::vector<SweepCollision>& collisions);

	[[nodiscard]] static V2_float GetRemainingVelocity(
		const V2_float& velocity, const DynamicCollision& c, CollisionResponse response
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

	void Init() {
		/* Possibly add stuff here in the future. */
	}

	void Shutdown();

	void Update(ecs::Manager& manager);

private:
	constexpr static float slop{ 0.005f };
};

} // namespace impl

} // namespace ptgn