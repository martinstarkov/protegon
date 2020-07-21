#pragma once

#include "System.h"

class DragSystem : public System<DragComponent, MotionComponent> {
public:
	virtual void update() override final;
};