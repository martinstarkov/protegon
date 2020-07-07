#pragma once

#include "Component.h"

#include "../../Vec2D.h"

// CONSIDER: Move speed elsewhere?

struct PlayerController : public Component<PlayerController> {
	Vec2D _speed;
	Vec2D _originalSpeed;
	PlayerController(Vec2D speed = Vec2D()) : _speed(speed), _originalSpeed(_speed) {}
};