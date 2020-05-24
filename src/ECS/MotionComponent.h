#pragma once
#include "Components.h"
#include "../Vec2D.h"

class MotionComponent : public Component {
public:
	MotionComponent(Vec2D velocity = Vec2D(), Vec2D terminalVelocity = Vec2D().infinite(), Vec2D acceleration = Vec2D()) {
		_velocity = velocity;
		_terminalVelocity = terminalVelocity;
		_acceleration = acceleration;
	}
	Vec2D getVelocity() { return _velocity; }
	void setVelocity(Vec2D velocity) {
		_velocity = velocity;
	}
	void addVelocity(Vec2D deltaVelocity) {
		_velocity += deltaVelocity;
	}
	Vec2D getTerminalVelocity() { return _terminalVelocity; }
	void setTerminalVelocity(Vec2D terminalVelocity) {
		_terminalVelocity = terminalVelocity;
	}
	Vec2D getAcceleration() { return _acceleration; }
	void setAcceleration(Vec2D acceleration) {
		_acceleration = acceleration;
	}
	void addAcceleration(Vec2D deltaAcceleration) {
		_acceleration += deltaAcceleration;
	}
private:
	Vec2D _velocity;
	Vec2D _terminalVelocity;
	Vec2D _acceleration;
};