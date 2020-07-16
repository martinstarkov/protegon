#pragma once

#include "Component.h"

#include "../../Vec2D.h"

struct MotionComponent : public Component<MotionComponent> {
	Vec2D velocity;
	Vec2D terminalVelocity;
	Vec2D acceleration;
	MotionComponent(Vec2D velocity = Vec2D(), Vec2D acceleration = Vec2D(), Vec2D terminalVelocity = Vec2D().infinite()) : velocity(velocity), acceleration(acceleration), terminalVelocity(terminalVelocity) {}
	virtual void init() override final;
};