#pragma once

#include <vector>

#include "components/collider.h"
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

enum class DynamicCollisionResponse {
	Slide,
	Bounce,
	Push
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

	// @return Final velocity of the object to prevent them from colliding with the manager objects.
	static V2_float Sweep(
		ecs::Entity entity, ecs::Manager& manager, DynamicCollisionResponse response,
		bool debug_draw = false
	);

private:
	struct SweepCollision {
		SweepCollision() = default;

		SweepCollision(const DynamicCollision& c, float dist2) : c{ c }, dist2{ dist2 } {}

		DynamicCollision c;
		float dist2{ 0.0f };
	};

	static void SortCollisions(std::vector<SweepCollision>& collisions);

	[[nodiscard]] static V2_float GetRemainingVelocity(
		const V2_float& velocity, const DynamicCollision& c, DynamicCollisionResponse response
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
	[[nodiscard]] static bool CanCollide(
		ecs::Entity e1, ecs::Entity e2, const BoxCollider& b1, const BoxCollider& b2
	) {
		if (!b1.enabled) {
			return false;
		}
		if (!b2.enabled) {
			return false;
		}
		if (e1 == e2) {
			return false;
		}
		if (!e1.IsAlive()) {
			return false;
		}
		if (!e2.IsAlive()) {
			return false;
		}
		return (b1.mask & b2.category) == b1.mask && (b2.mask & b1.category) == b2.mask;
	}
};

} // namespace impl

} // namespace ptgn