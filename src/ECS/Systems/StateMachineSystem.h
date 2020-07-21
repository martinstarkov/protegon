#pragma once

#include "System.h"

class StateMachineSystem : public System<StateMachineComponent> {
public:
	virtual void update() override final;
};