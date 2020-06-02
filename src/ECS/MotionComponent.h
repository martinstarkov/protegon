#pragma once
#include "Component.h"
#include "../Vec2D.h"

struct MotionComponent : public Component {
	static ComponentID ID;
	Vec2D _velocity;
	Vec2D _terminalVelocity;
	Vec2D _acceleration;
	MotionComponent(EntityID id, Vec2D velocity = Vec2D(), Vec2D acceleration = Vec2D(), Vec2D terminalVelocity = Vec2D().infinite()) : _velocity(velocity), _acceleration(acceleration), _terminalVelocity(terminalVelocity) {
		ID = createComponentID<MotionComponent>();
		_entityID = id;
	}
};

ComponentID MotionComponent::ID = 0;