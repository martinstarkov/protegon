#pragma once

#include "core/math/raycast.h"
#include "core/math/vector2.h"
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
};

} // namespace impl

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