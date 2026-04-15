#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "core/math/geometry/shape.h"
#include "runtime/ecs/entity.h"
#include "runtime/physics/collision.h"
#include "serialization/serialize.h"

namespace ptgn {

class CollisionHandler;

using ColliderMask = std::int64_t;

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

	PTGN_REFLECT(Collider, shape, mode, response, mask_, collides_with_masks_)

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