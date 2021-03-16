#pragma once

#include "Component.h"

#include "math/Vector2.h"

// TODO: Remember to make init function if going back to 
// serialization where object is created and values are set.

struct TransformComponent {
	V2_double position{};
	V2_double original_position;
	V2_double* center_of_rotation{ nullptr };
	double rotation{ 0.0 };
	TransformComponent() = default;
	~TransformComponent() {
		delete center_of_rotation;
		center_of_rotation = nullptr;
	}
	TransformComponent(const V2_double& position) : 
		position{ position }, original_position{ this->position } {
	}
	TransformComponent(const V2_double& position, const V2_double& center_of_rotation, double radius) : 
		position{ position }, 
		original_position{ this->position }, 
		center_of_rotation{ new V2_double{ center_of_rotation } } {
	}
	void ResetPosition() {
		position = original_position;
	}
};