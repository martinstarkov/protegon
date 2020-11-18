#pragma once

#include "Component.h"

#include "utils/Vector2.h"

struct TransformComponent {
	V2_double position{};
	V2_double original_position{};
	V2_double* center_of_rotation = nullptr;
	double rotation = 0.0;
	TransformComponent() = default;
	~TransformComponent() {
		delete center_of_rotation;
		center_of_rotation = nullptr;
	}
	TransformComponent(V2_double position) : position{ position } {}
	TransformComponent(V2_double position, V2_double center_of_rotation, double rotation = 0.0) : position{ position }, center_of_rotation{ new V2_double{ center_of_rotation }
}, rotation{ rotation } {
		Init();
	}
	void ResetPosition() {
		position = original_position;
	}
	void Init() {
		original_position = position;
	}
};