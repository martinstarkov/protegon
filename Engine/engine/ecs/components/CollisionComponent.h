#pragma once

#include "Component.h"

#include "renderer/AABB.h"
#include "renderer/Circle.h"

// TODO: Consider holding a pointer to the TransformComponent here instead of an AABB somehow?
// CONSIDERATIONS ^: How can you default construct a CollisionComponent? For serialization..

struct CollisionComponent {
	AABB collider;
	CollisionComponent(AABB collider = {}) : collider{ collider } {}
	CollisionComponent(V2_double position, V2_double size) : collider{ position, size } {}
};