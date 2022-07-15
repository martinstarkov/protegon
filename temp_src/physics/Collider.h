#pragma once

#include <vector> // std::vector

#include "physics/Manifold.h"
#include "core/ECS.h"

namespace ptgn {

namespace component {

struct Collider {
	Collider() = default;
	~Collider() = default;
	void Clear() {
		manifolds.clear();
		entities.clear();
	}
	bool collideable{ true };
	std::vector<Manifold> manifolds;
	std::vector<ecs::Entity> entities;
};

} // namespace component

} // namespace ptgn