#pragma once
#include "Component.h"

struct LifetimeComponent : public Component<LifetimeComponent> {
	bool _isDying;
	float _lifetime;
	LifetimeComponent(float lifetime = 1.0f, bool isDying = true) : _lifetime(lifetime), _isDying(isDying) {}
};