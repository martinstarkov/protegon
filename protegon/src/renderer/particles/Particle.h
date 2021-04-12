#pragma once

#include "math/Vector2.h"
#include "renderer/Color.h"

namespace engine {

struct Particle {
	V2_double position;
	V2_double velocity;
	V2_double acceleration;
	Color start_color{ colors::WHITE };
	Color end_color{ colors::BLACK };
	double start_radius{ 0.0 };
	double end_radius{ 5.0 };
	double rotation{ 0.0 };
	double angular_velocity{ 0.0 };
	double lifetime{ 1.0 };
};

}