#pragma once

#include <cstdint>
#include <vector>

#include "ecs/entity.h"
#include "math/geometry/shape.h"
#include "math/vector2.h"
#include "serialization/json/enum.h"

namespace ptgn {

struct Transform;

struct Collision {
	Collision() = default;

	Collision(const Entity& other, const V2_float& collision_normal) :
		entity{ other }, normal{ collision_normal } {}

	operator bool() const {
		return entity != Entity{};
	}

	Entity entity;
	// Normal set to {} for overlap only collisions.
	V2_float normal;

	friend bool operator==(const Collision& a, const Collision& b) {
		return a.entity == b.entity;
	}

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Collision, entity, normal)
};

namespace impl {

class CollisionHandler;

} // namespace impl

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

enum class CollisionMode {
	None,		// No collision checks.
	Overlap,	// Overlap checks.
	Discrete,	// Discrete collision detection.
	Continuous, // Continuous collision detection for high velocity colliders.
};

struct Collider {
	Collider() = default;

	Collider(const ColliderShape& shape);

	ColliderShape shape;

	CollisionMode mode{ CollisionMode::Discrete };

	// How the velocity of the sweep should respond to obstacles.
	// Only applicable if mode != CollisionMode::Overlap.
	CollisionResponse response{ CollisionResponse::Slide };

	Collider& SetOverlapMode();

	Collider& SetCollisionMode(CollisionMode new_mode = CollisionMode::Discrete);

	[[nodiscard]] CollisionCategory GetCollisionCategory() const;

	void SetCollisionCategory(const CollisionCategory& category);

	void ResetCollisionCategory();

	// Allow collider to collide with anything.
	void ResetCollidesWith();

	[[nodiscard]] bool CanCollideWith(const CollisionCategory& category) const;

	[[nodiscard]] bool IsCategory(const CollisionCategory& category) const;

	void AddCollidesWith(const CollisionCategory& category);

	void RemoveCollidesWith(const CollisionCategory& category);

	void SetCollidesWith(const CollidesWithCategories& categories);

	// @return Empty collision if the entities have not collided during this frame, or the
	// collision.
	[[nodiscard]] Collision IntersectedWith(const Entity& other) const;
	[[nodiscard]] Collision SweptWith(const Entity& other) const;
	[[nodiscard]] bool OverlappedWith(const Entity& other) const;

	// TODO: Fix collider shape serialization: KeyValue("shape", shape)

	PTGN_SERIALIZER_REGISTER_NAMED(
		Collider, KeyValue("mode", mode), KeyValue("response", response), KeyValue("mask", mask_),
		KeyValue("category", category_)
	)

private:
	friend class impl::CollisionHandler;

	void ResetContainers();

	void ResetOverlaps();
	void ResetIntersects();
	void ResetSweeps();

	void AddOverlap(const Entity& other);
	void AddIntersect(const Collision& collision);
	void AddSweep(const Collision& collision);

	// Which categories this collider collides with.
	std::vector<CollisionCategory> mask_;

	// Which category this collider is a part of.
	CollisionCategory category_{ 0 };

	// Collisions from the current frame.
	std::vector<Entity> overlaps_;
	std::vector<Collision> intersects_;
	std::vector<Collision> sweeps_;

	// Collisions from the previous frame.
	std::vector<Entity> previous_overlaps_;
	std::vector<Collision> previous_intersects_;
	std::vector<Collision> previous_sweeps_;
};

PTGN_SERIALIZER_REGISTER_ENUM(
	CollisionResponse, { { CollisionResponse::Slide, "slide" },
						 { CollisionResponse::Bounce, "bounce" },
						 { CollisionResponse::Push, "push" },
						 { CollisionResponse::Stick, "stick" } }
);

PTGN_SERIALIZER_REGISTER_ENUM(
	CollisionMode, { { CollisionMode::None, nullptr },
					 { CollisionMode::Overlap, "overlap" },
					 { CollisionMode::Discrete, "discrete" },
					 { CollisionMode::Continuous, "continuous" } }
);

} // namespace ptgn