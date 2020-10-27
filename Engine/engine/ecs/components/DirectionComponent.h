#pragma once

#include "Component.h"

#include "utils/Direction.h"

struct DirectionComponent {
	Direction direction;
	Direction previous_direction;
	DirectionComponent(Direction direction = Direction::DOWN) : direction{ direction } {
		Init();
	}
	void Init() {
		previous_direction = direction;
	}
};