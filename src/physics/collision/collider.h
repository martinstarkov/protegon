#pragma once

#include <cstdint>
#include <functional>
#include <unordered_set>
#include <vector>

#include "core/entity.h"
#include "math/vector2.h"
#include "rendering/api/origin.h"
#include "serialization/enum.h"

namespace ptgn {

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

	// Collisions from the current frame (updated after calling game.collision.Update()).
	std::unordered_set<Collision> collisions;

	// Collisions from the previous frame.
	std::unordered_set<Collision> prev_collisions;

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
	[[nodiscard]] bool ProcessCallback(Entity e1, Entity e2);

	[[nodiscard]] bool CanCollideWith(const CollisionCategory& category) const;

	[[nodiscard]] bool IsCategory(const CollisionCategory& category) const;

	void AddCollidesWith(const CollisionCategory& category);

	void RemoveCollidesWith(const CollisionCategory& category);

	void SetCollidesWith(const CollidesWithCategories& categories);

	void InvokeCollisionCallbacks(Entity& entity);

	PTGN_SERIALIZER_REGISTER_NAMED(
		Collider, KeyValue("collisions", collisions), KeyValue("prev_collisions", prev_collisions),
		KeyValue("overlap_only", overlap_only), KeyValue("continuous", continuous),
		KeyValue("response", response), KeyValue("mask", mask_), KeyValue("category", category_)
	)
protected:
	friend class impl::CollisionHandler;

	void ResetCollisions();

	// Which categories this collider collides with.
	std::unordered_set<CollisionCategory> mask_;
	// Which category this collider is a part of.
	CollisionCategory category_{ 0 };
};

struct BoxCollider : public Collider {
	BoxCollider() = default;

	explicit BoxCollider(const V2_float& collider_size, Origin collider_origin = Origin::Center) :
		size{ collider_size }, origin{ collider_origin } {}

	V2_float size;
	Origin origin{ Origin::Center };

	PTGN_SERIALIZER_REGISTER_NAMED(
		BoxCollider, KeyValue("collisions", collisions),
		KeyValue("prev_collisions", prev_collisions), KeyValue("overlap_only", overlap_only),
		KeyValue("continuous", continuous), KeyValue("response", response), KeyValue("mask", mask_),
		KeyValue("category", category_), KeyValue("size", size), KeyValue("origin", origin)
	)
};

struct CircleCollider : public Collider {
	CircleCollider() = default;

	explicit CircleCollider(float collider_radius) : radius{ collider_radius } {}

	float radius{ 0.0f };

	PTGN_SERIALIZER_REGISTER_NAMED(
		CircleCollider, KeyValue("collisions", collisions),
		KeyValue("prev_collisions", prev_collisions), KeyValue("overlap_only", overlap_only),
		KeyValue("continuous", continuous), KeyValue("response", response), KeyValue("mask", mask_),
		KeyValue("category", category_), KeyValue("radius", radius)
	)
};

// struct PolygonCollider : public Collider {
//	std::vector<V2_float> vertices;
// };

// TODO: Add edge collider.

PTGN_SERIALIZER_REGISTER_ENUM(
	CollisionResponse, { { CollisionResponse::Slide, "slide" },
						 { CollisionResponse::Bounce, "bounce" },
						 { CollisionResponse::Push, "push" },
						 { CollisionResponse::Stick, "stick" } }
);

} // namespace ptgn