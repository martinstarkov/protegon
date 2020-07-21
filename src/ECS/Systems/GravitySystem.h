#pragma once

#include "System.h"

class GravitySystem : public System<MotionComponent, GravityComponent> {
public:
	virtual void update() override final;
};