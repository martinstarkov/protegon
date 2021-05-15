#pragma once

#include <vector>
#include <unordered_set>

#include "ecs/ECS.h"
#include "physics/Manifold.h"

// TODO: Reconsider offset application.
// TODO: Don't store collisions in a vector???

namespace engine {

using CollisionFunction = void(*)(ecs::Entity& target, const engine::Manifold& manifold);

struct HitboxComponent {
	HitboxComponent() = default;
	HitboxComponent(const V2_int& offset) : offset{ offset } {}
	// The offset of a hitbox from the position of the shape.
	// For circles this offset is taken from the center.
	// For AABBs this offset is taken from the top left.
	V2_int offset;
	// A vector of the entities and their respect collision informations 
	// for collisions occured during the frame. 
	// Unresolved collisions are kept in the vector onto the next frame.
	std::vector<std::pair<ecs::Entity, engine::Manifold>> collisions;
	// Tag component ids to be ignored by the entity when checking for collisions.
	std::unordered_set<std::size_t> ignored_tags;
	// Collision resolution function to be called for each collision.
	// Nullptr by default. This means no custom resolution is applied.
	CollisionFunction resolution_function{ nullptr };
};

} // namespace engine