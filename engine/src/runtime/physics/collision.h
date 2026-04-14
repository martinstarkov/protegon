#pragma once

#include <ostream>

#include "core/math/raycast.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

namespace impl {

struct SweepCollision {
	SweepCollision() = default;

	SweepCollision(
		const RaycastResult& raycast_result, float distance_squared, Entity sweep_entity
	);

	/// @brief CollisionInfo entity.
	Entity entity;
	RaycastResult collision;
	float dist2{ 0.0f };

	friend std::ostream& operator<<(std::ostream& os, const SweepCollision& sweep_collision) {
		os << "{ entity: " << sweep_collision.entity;
		os << ", collision: " << sweep_collision.collision;
		os << ", dist2: " << sweep_collision.dist2 << " }";
		return os;
	}
};

} // namespace impl

} // namespace ptgn