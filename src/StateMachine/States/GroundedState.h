#pragma once

#include "State.h"

class GroundedState : public State<GroundedState> {
	virtual void update() override final;
};