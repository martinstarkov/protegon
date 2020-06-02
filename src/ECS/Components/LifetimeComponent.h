#pragma once
#include "Component.h"

struct LifetimeComponent : public Component {
	static ComponentID ID;
	bool _isDying;
	float _lifetime;
	LifetimeComponent(float lifetime = 1.0f, bool isDying = true) : _lifetime(lifetime), _isDying(isDying) {
		ID = createComponentID<LifetimeComponent>();
	}
};

ComponentID LifetimeComponent::ID = 0;