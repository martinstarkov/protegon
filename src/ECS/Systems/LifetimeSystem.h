#pragma once

#include "System.h"

// TODO: Make it clearer how this system acts as a component (health vs bullet lifetime vs block lifetime)

class LifetimeSystem : public System<LifetimeComponent> {
public:
	virtual void update() override final;
};