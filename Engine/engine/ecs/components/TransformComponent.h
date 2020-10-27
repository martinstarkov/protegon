#pragma once

#include "Component.h"

#include "utils/Vector2.h"

struct TransformComponent {
	V2_double position;
	V2_double original_position;
	TransformComponent(V2_double position = { 0.0, 0.0 }) : position{ position } {
		Init();
	}
	void ResetPosition() {
		position = original_position;
	}
	void Init() {
		original_position = position;
	}
};