#pragma once

#include "Component.h"

#include "../../Vec2D.h"

// CONSIDER: Move speed elsewhere?

struct PlayerController : public Component<PlayerController> {
	Vec2D _speed;
	PlayerController(Vec2D speed = Vec2D()) : _speed(speed) {}
};