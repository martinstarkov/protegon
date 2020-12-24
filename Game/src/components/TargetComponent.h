#pragma once

#include <engine/Include.h>

struct TargetComponent {
	TargetComponent(V2_double target_position, double approach_speed) : target_position{ target_position }, approach_speed{ approach_speed } {}
	V2_double target_position;
	double approach_speed;
};

