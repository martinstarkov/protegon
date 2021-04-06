#pragma once

#include "Component.h"

struct LifetimeComponent {
	const double original_lifetime; // seconds
	double lifetime; // seconds
	bool is_dying; // lose lifetime every frame
	LifetimeComponent(double lifetime = 1.0, bool is_dying = true) : lifetime{ lifetime }, original_lifetime{ lifetime }, is_dying{ is_dying } {}
};