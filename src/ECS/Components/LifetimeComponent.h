#pragma once

#include "Component.h"

struct LifetimeComponent : public Component<LifetimeComponent> {
	bool isDying;
	double lifetime;
	LifetimeComponent(double lifetime = 1.0, bool isDying = true) : lifetime(lifetime), isDying(isDying) {}
};