#pragma once

#include <vector> // std::vector

#include "physics/Manifold.h"

namespace ptgn {

namespace component {

struct Collider {
	Collider() = default;
	~Collider() = default;
	void Clear() {
		manifolds.clear();
	}
	bool collideable{ true };
	std::vector<Manifold> manifolds;
};

} // namespace component

} // namespace ptgn