#pragma once

#include <engine/Include.h>

struct TargetComponent {
	TargetComponent(ecs::Entity target, V2_double target_position, double approach_speed) : target{ target }, target_position { target_position }, approach_speed{ approach_speed } {}
	ecs::Entity target;
	V2_double target_position;
	double approach_speed;
};

