#pragma once

#include <functional>
#include <span>
#include <vector>

#include "core/math/geometry/shape.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_filter.h"
#include "runtime/physics/collision.h"
#include "serialization/serialize.h"

namespace ptgn {

class CollisionHandler;

enum class CollisionResponse {
	Slide,	/// Velocity set perpendicular to collision normal at same speed.
	Bounce, /// Velocity set at 45 degrees to collision normal.
	Push,	/// Velocity set perpendicular to collision normal at partial speed.
	Stick	/// Velocity set to 0.
};
PTGN_REFLECT_ENUM(CollisionResponse);

enum class CollisionMode {
	None,		/// No collision checks.
	Overlap,	/// Overlap checks.
	Discrete,	/// Discrete collision detection.
	Continuous, /// Continuous collision detection for high velocity colliders.
};
PTGN_REFLECT_ENUM(CollisionMode);

struct Collider {
	Collider() = default;

	explicit Collider(const ColliderShape& collider_shape);

	ColliderShape shape{};

	CollisionMode mode{ CollisionMode::Discrete };

	/// @brief How the velocity of the sweep should respond to obstacles.
	/// Only applicable if mode != CollisionMode::Overlap.
	CollisionResponse response{ CollisionResponse::Slide };

	/// @brief Selects which candidate entities this collider can collide or overlap with.
	EntityFilter collides_with{};

	Collider& SetOverlapMode();

	Collider& SetCollisionMode(CollisionMode new_mode = CollisionMode::Discrete);

	// @return Empty collision if the entities have not collided during this frame, or the
	// collision.
	[[nodiscard]] CollisionInfo IntersectedWith(Entity other) const;
	[[nodiscard]] CollisionInfo SweptWith(Entity other) const;
	[[nodiscard]] bool OverlappedWith(Entity other) const;

	[[nodiscard]] std::span<const Entity> GetOverlaps() const;
	[[nodiscard]] std::span<const CollisionInfo> GetIntersections() const;
	[[nodiscard]] std::span<const CollisionInfo> GetSweeps() const;

	/// @brief Optional function to check for early outs before performing collision checks. Should
	/// return true if the collision check should be performed, false if it should be skipped.
	std::function<bool(Entity, Entity)> pre_collision_check{};

	/// @brief Optional function to check for early outs before performing overlap checks. Should
	/// return true if the overlap check should be performed, false if it should be skipped.
	std::function<bool(Entity, Entity)> pre_overlap_check{};

	PTGN_REFLECT(Collider, shape, mode, response, collides_with)

private:
	friend class CollisionHandler;

	void ResetContainers();

	void ResetOverlaps();
	void ResetIntersects();
	void ResetSweeps();

	void AddOverlap(Entity other);
	void AddIntersect(const CollisionInfo& collision);
	void AddSweep(const CollisionInfo& collision);

	/// @brief Collisions from the current frame.
	std::vector<Entity> overlaps_{};
	std::vector<CollisionInfo> intersects_{};
	std::vector<CollisionInfo> sweeps_{};

	/// @brief Collisions from the previous frame.
	std::vector<Entity> previous_overlaps_{};
	std::vector<CollisionInfo> previous_intersects_{};
	std::vector<CollisionInfo> previous_sweeps_{};
};

} // namespace ptgn
