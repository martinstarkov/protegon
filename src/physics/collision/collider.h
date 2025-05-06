#pragma once

#include <cstdint>
#include <functional>
#include <unordered_set>
#include <vector>

#include "core/entity.h"
#include "math/geometry/circle.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"

namespace ptgn {

namespace impl {

class CollisionHandler;

} // namespace impl

struct Collision {
	Collision() = default;

	Collision(Entity e1, Entity e2, const V2_float& normal) :
		entity1{ e1 }, entity2{ e2 }, normal{ normal } {}

	Entity entity1;
	Entity entity2;
	// Normal set to {} for overlap only collisions.
	V2_float normal;

	bool operator==(const Collision& o) const;

	bool operator!=(const Collision& o) const;
};

} // namespace ptgn

template <>
struct std::hash<ptgn::Collision> {
	std::size_t operator()(const ptgn::Collision& c) const noexcept {
		// Hashing combination algorithm from:
		// https://stackoverflow.com/a/17017281
		std::size_t hash{ 17 };
		hash = hash * 31 + c.entity1.GetHash();
		hash = hash * 31 + c.entity2.GetHash();
		hash = hash * 31 + std::hash<ptgn::V2_float>()(c.normal);
		return hash;
	}
};

namespace ptgn {

using CollisionCategory		 = std::int64_t;
using CollidesWithCategories = std::vector<CollisionCategory>;
using CollisionCallback		 = std::function<void(Collision)>;

enum class CollisionResponse {
	Slide,
	Bounce,
	Push
};

struct Collider {
	// TODO: Implement local physics world bounds.
	// Rect bounds;

	// Collisions from the current frame (updated after calling game.collision.Update()).
	std::unordered_set<Collision> collisions;

	// Collisions from the previous frame.
	std::unordered_set<Collision> prev_collisions;

	// Must return true for collisions to be checked.
	std::function<bool(Entity, Entity)> before_collision;

	CollisionCallback on_collision_start;
	CollisionCallback on_collision;
	CollisionCallback on_collision_stop;

	// Overwrites continuous/regular collision in favor of overlap checks.
	bool overlap_only{ false };

	// Continuous collision detection for high velocity colliders.
	bool continuous{ false };

	// How the velocity of the sweep should respond to obstacles.
	// Not applicable if continuous is set to false.
	CollisionResponse response{ CollisionResponse::Slide };

	[[nodiscard]] CollisionCategory GetCollisionCategory() const;

	void SetCollisionCategory(const CollisionCategory& category);

	void ResetCollisionCategory();

	// Allow collider to collide with anything.
	void ResetCollidesWith();

	// May invalidate all existing component references.
	[[nodiscard]] bool ProcessCallback(Entity e1, Entity e2);

	[[nodiscard]] bool CanCollideWith(const CollisionCategory& category) const;

	[[nodiscard]] bool IsCategory(const CollisionCategory& category) const;

	void AddCollidesWith(const CollisionCategory& category);

	void RemoveCollidesWith(const CollisionCategory& category);

	void SetCollidesWith(const CollidesWithCategories& categories);

	void InvokeCollisionCallbacks();

private:
	friend class impl::CollisionHandler;

	void ResetCollisions();

	// Which categories this collider collides with.
	std::unordered_set<CollisionCategory> mask_;
	// Which category this collider is a part of.
	CollisionCategory category_{ 0 };
};

struct BoxCollider : public Rect, public Collider {
	BoxCollider() = delete;
	using Rect::Rect;
};

struct CircleCollider : public Circle, public Collider {
	CircleCollider() = delete;
	using Circle::Circle;
};

// struct PolygonCollider : public Collider {
//	std::vector<V2_float> vertices;
// };

// TODO: Add edge collider.

} // namespace ptgn