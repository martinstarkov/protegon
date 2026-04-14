#pragma once

#include "runtime/ecs/entity.h"
#include "runtime/physics/collider.h"

namespace ptgn::event {

struct Collision {
	operator CollisionInfo() const { // NOSONAR
		return collision;
	}

	CollisionInfo collision;
};

struct OverlapStart {
	operator Entity() const { // NOSONAR
		return overlap_entity;
	}

	Entity overlap_entity;
};

struct Overlap {
	operator Entity() const { // NOSONAR
		return overlap_entity;
	}

	Entity overlap_entity;
};

struct OverlapStop {
	operator Entity() const { // NOSONAR
		return overlap_entity;
	}

	Entity overlap_entity;
};

} // namespace ptgn::event