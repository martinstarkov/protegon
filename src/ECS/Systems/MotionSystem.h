#pragma once

#include "System.h"

class MotionSystem : public System<MotionComponent> {
public:
	virtual void update() override final;
};