#pragma once
#include "Component.h"
#include "../../Vec2D.h"

struct MotionComponent : public Component<MotionComponent> {
	Vec2D _velocity;
	Vec2D _terminalVelocity;
	Vec2D _acceleration;
	MotionComponent(Vec2D velocity = Vec2D(), Vec2D acceleration = Vec2D(), Vec2D terminalVelocity = Vec2D().infinite()) : _velocity(velocity), _acceleration(acceleration), _terminalVelocity(terminalVelocity) {
	}
};

//ComponentID MotionComponent::ID = 0;