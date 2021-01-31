#pragma once

#include "math/Vector2.h"
#include "renderer/Color.h"

namespace engine {

struct Particle {
	V2_double position;
	V2_double velocity;
	V2_double acceleration;
	double rotation = 0.0;
	Color start_color = engine::WHITE, end_color = engine::BLACK;
	double start_radius = 0.0, end_radius = 5.0, angular_velocity = 0.0;
	double lifetime = 1.0;
};

}