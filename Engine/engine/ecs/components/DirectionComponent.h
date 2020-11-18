#pragma once

#include "Component.h"

#include "utils/Direction.h"

struct DirectionComponent {
	Direction x_direction;
	Direction y_direction;
	Direction x_previous_direction;
	Direction y_previous_direction;
	DirectionComponent(Direction x_direction = Direction::RIGHT) : x_direction{ x_direction }, y_direction{ y_direction } {
		Init();
	}
	void Init() {
		x_previous_direction = x_direction;
		y_previous_direction = y_direction;
	}
};