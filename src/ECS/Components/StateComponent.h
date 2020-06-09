#pragma once

#include "Component.h"

#include "../StateMachine.h"

struct StateComponent : public Component<StateComponent> {
	StateMachine _sm;
	StateComponent(Entity& entity) : _sm(entity) {}
};