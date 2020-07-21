#pragma once

#include "System.h"

struct MotionComponent;
struct GravityComponent;

class GravitySystem : public System<MotionComponent, GravityComponent> {
public:
	virtual void update() override final;
};