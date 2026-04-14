#pragma once

#include <cstdint>
#include <functional>
#include <ostream>
#include <vector>

#include "core/math/geometry/shape.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "serialization/serialize.h"

namespace ptgn {

class CollisionHandler;

struct CollisionInfo {
	CollisionInfo() = default;

	CollisionInfo(Entity other, V2_float collision_normal) :
		entity{ other }, normal{ collision_normal } {}

	operator bool() const {
		return entity.operator bool();
	}

	Entity entity;
	/// @brief Normal set to {} for overlap only collisions.
	V2_float normal;

	friend bool operator==(const CollisionInfo& a, const CollisionInfo& b) {
		return a.entity == b.entity;
	}

	friend std::ostream& operator<<(std::ostream& os, const CollisionInfo& collision) {
		os << "{ entity: " << collision.entity;
		os << ", normal: " << collision.normal << " }";
		return os;
	}

	PTGN_REFLECT(CollisionInfo, entity, normal)
};

} // namespace ptgn

template <>
struct std::hash<ptgn::CollisionInfo> {
	std::size_t operator()(const ptgn::CollisionInfo& c) const noexcept {
		// Hashing combination algorithm from:
		// https://stackoverflow.com/a/17017281
		std::size_t value{ 17 };
		value = value * 31 + c.entity.GetHash();
		value = value * 31 + std::hash<ptgn::V2_float>()(c.normal);
		return value;
	}
};

namespace ptgn {

using ColliderMask = std::int64_t;

enum class CollisionResponse {
	Slide,	/// Velocity set perpendicular to collision normal at same speed.
	Bounce, /// Velocity set at 45 degrees to collision normal.
	Push,	/// Velocity set perpendicular to collision normal at partial speed.
	Stick	/// Velocity set to 0.
};

std::ostream& operator<<(std::ostream& os, CollisionResponse response);

PTGN_REFLECT_ENUM(
	CollisionResponse, { { CollisionResponse::Slide, "slide" },
						 { CollisionResponse::Bounce, "bounce" },
						 { CollisionResponse::Push, "push" },
						 { CollisionResponse::Stick, "stick" } }
);

enum class CollisionMode {
	None,		/// No collision checks.
	Overlap,	/// Overlap checks.
	Discrete,	/// Discrete collision detection.
	Continuous, /// Continuous collision detection for high velocity colliders.
};

std::ostream& operator<<(std::ostream& os, CollisionMode mode);

PTGN_REFLECT_ENUM(
	CollisionMode, { { CollisionMode::None, nullptr },
					 { CollisionMode::Overlap, "overlap" },
					 { CollisionMode::Discrete, "discrete" },
					 { CollisionMode::Continuous, "continuous" } }
);

struct Collider {
	Collider() = default;

	explicit Collider(const ColliderShape& shape);

	ColliderShape shape;

	CollisionMode mode{ CollisionMode::Discrete };

	/// @brief  How the velocity of the sweep should respond to obstacles.
	/// Only applicable if mode != CollisionMode::Overlap.
	CollisionResponse response{ CollisionResponse::Slide };

	Collider& SetOverlapMode();

	Collider& SetCollisionMode(CollisionMode new_mode = CollisionMode::Discrete);

	ColliderMask GetMask() const;

	Collider& SetMask(ColliderMask mask);

	Collider& ResetMask();

	/// @brief  Allow collider to collide with anything.
	Collider& ResetCollidesWith();

	[[nodiscard]] bool CanCollideWith(ColliderMask mask) const;

	[[nodiscard]] bool IsMask(ColliderMask mask) const;

	Collider& AddCollidesWith(ColliderMask mask);

	Collider& RemoveCollidesWith(ColliderMask mask);

	Collider& SetCollidesWith(const std::vector<ColliderMask>& categories);

	// @return Empty collision if the entities have not collided during this frame, or the
	// collision.
	[[nodiscard]] CollisionInfo IntersectedWith(Entity other) const;
	[[nodiscard]] CollisionInfo SweptWith(Entity other) const;
	[[nodiscard]] bool OverlappedWith(Entity other) const;

	// TODO: Fix collider shape serialization: KeyValue("shape", shape)

	PTGN_REFLECT(
		Collider, KeyValue("mode", mode), KeyValue("response", response), KeyValue("mask", mask_),
		KeyValue("collides_with_masks_", collides_with_masks_)
	)

	/// @brief Optional function to check for early outs before performing collision checks. Should
	/// return true if the collision check should be performed, false if it should be skipped.
	std::function<bool(Entity, Entity)> pre_collision_check;

	/// @brief Optional function to check for early outs before performing overlap checks. Should
	/// return true if the overlap check should be performed, false if it should be skipped.
	std::function<bool(Entity, Entity)> pre_overlap_check;

private:
	friend class CollisionHandler;

	void ResetContainers();

	void ResetOverlaps();
	void ResetIntersects();
	void ResetSweeps();

	void AddOverlap(Entity other);
	void AddIntersect(const CollisionInfo& collision);
	void AddSweep(const CollisionInfo& collision);

	/// @brief  Which categories this collider collides with.
	std::vector<ColliderMask> collides_with_masks_;

	/// @brief  Which mask this collider is a part of.
	ColliderMask mask_{ 0 };

	/// @brief  Collisions from the current frame.
	std::vector<Entity> overlaps_;
	std::vector<CollisionInfo> intersects_;
	std::vector<CollisionInfo> sweeps_;

	/// @brief  Collisions from the previous frame.
	std::vector<Entity> previous_overlaps_;
	std::vector<CollisionInfo> previous_intersects_;
	std::vector<CollisionInfo> previous_sweeps_;
};

} // namespace ptgn