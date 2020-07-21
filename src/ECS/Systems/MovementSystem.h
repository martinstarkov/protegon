#pragma once

#include "System.h"

struct TransformComponent;
struct MotionComponent;

class MovementSystem : public System<TransformComponent, MotionComponent> {
public:
	virtual void update() override final;
};