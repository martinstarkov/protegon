#pragma once

#include "System.h"

struct MotionComponent;

class MotionSystem : public System<MotionComponent> {
public:
	virtual void update() override final;
};