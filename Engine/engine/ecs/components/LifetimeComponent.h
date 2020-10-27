#pragma once

#include "Component.h"

struct LifetimeComponent {
	double lifetime; // seconds
	bool is_dying; // lose lifetime every frame
	LifetimeComponent(double lifetime = 1.0, bool is_dying = true) : lifetime{ lifetime }, is_dying{ is_dying } {}
};