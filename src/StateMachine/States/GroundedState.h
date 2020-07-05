#pragma once

#include "State.h"

class GroundedState : public State<GroundedState> {
public:
	virtual void onEnter() override;
	virtual void onExit() override;
	virtual void update() override;
};