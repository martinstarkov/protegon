#pragma once

#include "System.h"

class MovementSystem : public System<TransformComponent, MotionComponent> {
public:
	virtual void update() override final;
};