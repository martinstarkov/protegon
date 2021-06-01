#pragma once

#include <cstdlib> // std::size_t
#include <unordered_set> // std::unordered_set

#include "core/ECS.h"
#include "math/Vector2.h"
#include "physics/Manifold.h"

namespace ptgn {

struct HitboxComponent {

	using CollisionFunction = void(*)(ecs::Entity& target, const Manifold& manifold);

	HitboxComponent() = default;
	~HitboxComponent() = default;
	HitboxComponent(const V2_int& offset) : offset{ offset } {}

	// Resolves a collision if the resolution_function is defined.
	void Resolve(ecs::Entity& target, const Manifold& manifold);

	/*
	* @return True if hitbox should collide with entity based on its TagComponent, false otherwise.
	*/
	bool CanCollideWith(const ecs::Entity& entity);

	// The offset of a hitbox from the position of the shape.
	// For circles this offset is taken from the center.
	// For AABBs this offset is taken from the top left.
	V2_int offset;

	// Tag component ids to be ignored by the entity when checking for collisions.
	std::unordered_set<std::size_t> ignored_tags;
	
	// Collision resolution function to be called for each collision.
	// Nullptr by default. This means no custom resolution is applied.
	CollisionFunction resolution_function{ nullptr };
};

} // namespace ptgn