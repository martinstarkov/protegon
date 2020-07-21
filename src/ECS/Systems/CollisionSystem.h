#pragma once

#include "System.h"

// TODO: Redo entire collision system

class CollisionSystem : public System<TransformComponent, SizeComponent, CollisionComponent> {
public:
	virtual void update() override final;
};