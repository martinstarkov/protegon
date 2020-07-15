#pragma once

#include "Component.h"

#include "../../AABB.h"

// TODO: Figure out what data belongs here

struct CollisionComponent : public Component<CollisionComponent> {
	bool colliding;
	CollisionComponent() : colliding(false) {}
};