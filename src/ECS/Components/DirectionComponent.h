#pragma once

#include "Component.h"

#include "../../Direction.h"

struct DirectionComponent : public Component<DirectionComponent> {
	Direction direction;
	Direction previousDirection;
	DirectionComponent(Direction direction = Direction::DOWN) : direction(direction), previousDirection(direction) {}
};