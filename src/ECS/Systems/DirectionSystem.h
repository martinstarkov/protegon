#pragma once

#include "System.h"

struct DirectionComponent;
struct MotionComponent;

class DirectionSystem : public System<DirectionComponent, MotionComponent> {
public:
	virtual void update() override final;
};