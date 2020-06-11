#pragma once

#include "Component.h"

#include "../../AABB.h"

struct CollisionComponent : public Component<CollisionComponent> {
	bool _colliding;
	CollisionComponent() : _colliding(false) {}
};