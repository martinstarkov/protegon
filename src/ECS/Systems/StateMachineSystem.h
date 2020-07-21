#pragma once

#include "System.h"

struct StateMachineComponent;

class StateMachineSystem : public System<StateMachineComponent> {
public:
	virtual void update() override final;
};