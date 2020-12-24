#pragma once

#include <cstdlib> // std::size_t

struct TowerComponent {
	// Firing delay in cycles.
	TowerComponent(std::size_t projectiles, double range, std::size_t firing_delay) : projectiles{ projectiles }, range{ range }, firing_delay{ firing_delay } {}
	std::size_t projectiles;
	double range;
	std::size_t firing_delay;
	std::size_t firing_counter = 0;
	// Set later once target is locked.
	ecs::Entity target = ecs::null;
};