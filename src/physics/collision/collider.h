#pragma once

#include <cstdint>
#include <vector>

#include "core/entity.h"
#include "math/geometry.h"
#include "math/vector2.h"
#include "serialization/enum.h"

namespace ptgn {

struct Transform;

// TODO: Move elsewhere once IScript is not tied to entity.
struct Collision {
	Collision() = default;

	Collision(const Entity& other, const V2_float& collision_normal) :
		entity{ other }, normal{ collision_normal } {}

	Entity entity;
	// Normal set to {} for overlap only collisions.
	V2_float normal;

	friend bool operator==(const Collision& a, const Collision& b) {
		return a.entity == b.entity;
	}

	friend bool operator!=(const Collision& a, const Collision& b) {
		return !operator==(a, b);
	}

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Collision, entity, normal)
};

namespace impl {

class CollisionHandler;

} // namespace impl

struct PhysicsBody : public Entity {
	using Entity::Entity;

	PhysicsBody(const Entity& entity) : Entity{ entity } {}

	// @return True if the current entity or any of its parent entities is immovable.
	[[nodiscard]] bool IsImmovable() const;

	[[nodiscard]] Transform& GetRootTransform();
};

} // namespace ptgn

template <>
struct std::hash<ptgn::Collision> {
	std::size_t operator()(const ptgn::Collision& c) const noexcept {
		// Hashing combination algorithm from:
		// https://stackoverflow.com/a/17017281
		std::size_t value{ 17 };
		value = value * 31 + c.entity.GetHash();
		value = value * 31 + std::hash<ptgn::V2_float>()(c.normal);
		return value;
	}
};

namespace ptgn {

using CollisionCategory		 = std::int64_t;
using CollidesWithCategories = std::vector<CollisionCategory>;

enum class CollisionResponse {
	Slide,	// Velocity set perpendicular to collision normal at same speed.
	Bounce, // Velocity set at 45 degrees to collision normal.
	Push,	// Velocity set perpendicular to collision normal at partial speed.
	Stick	// Velocity set to 0.
};

struct Collider {
	// TODO: Implement local physics world bounds.
	// Rect bounds;

	Collider() = default;

	Collider(const Shape& shape);

	Shape shape;

	// Overwrites continuous/regular collision in favor of overlap checks.
	bool overlap_only{ false };

	// Continuous collision detection for high velocity colliders.
	bool continuous{ false };

	Collider& SetOverlapOnly();

	// How the velocity of the sweep should respond to obstacles.
	// Not applicable if continuous is set to false.
	CollisionResponse response{ CollisionResponse::Slide };

	[[nodiscard]] CollisionCategory GetCollisionCategory() const;

	void SetCollisionCategory(const CollisionCategory& category);

	void ResetCollisionCategory();

	// Allow collider to collide with anything.
	void ResetCollidesWith();

	// May invalidate all existing component references.
	[[nodiscard]] bool ProcessCallback(Entity e1, Entity e2) const;

	[[nodiscard]] bool CanCollideWith(const CollisionCategory& category) const;

	[[nodiscard]] bool IsCategory(const CollisionCategory& category) const;

	void AddCollidesWith(const CollisionCategory& category);

	void RemoveCollidesWith(const CollisionCategory& category);

	void SetCollidesWith(const CollidesWithCategories& categories);

	void InvokeCollisionCallbacks(Entity& entity) const;

	PTGN_SERIALIZER_REGISTER_NAMED(
		Collider, KeyValue("shape", shape), KeyValue("collisions", collisions_),
		KeyValue("prev_collisions", prev_collisions_), KeyValue("overlap_only", overlap_only),
		KeyValue("continuous", continuous), KeyValue("response", response), KeyValue("mask", mask_),
		KeyValue("category", category_)
	)

private:
	friend class impl::CollisionHandler;

	void ResetCollisions();

	void AddCollision(const Collision& collision);

	// Which categories this collider collides with.
	std::vector<CollisionCategory> mask_;
	// Which category this collider is a part of.
	CollisionCategory category_{ 0 };

	// Collisions from the current frame (updated after calling game.collision.Update()).
	std::vector<Collision> collisions_;

	// Collisions from the previous frame.
	std::vector<Collision> prev_collisions_;
};

PTGN_SERIALIZER_REGISTER_ENUM(
	CollisionResponse, { { CollisionResponse::Slide, "slide" },
						 { CollisionResponse::Bounce, "bounce" },
						 { CollisionResponse::Push, "push" },
						 { CollisionResponse::Stick, "stick" } }
);

} // namespace ptgn