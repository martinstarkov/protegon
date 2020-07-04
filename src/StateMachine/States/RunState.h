#pragma once

#include "State.h"

class RunState : public State<RunState> {
public:
	virtual void onEnter() override;
	virtual void onExit() override;
	virtual void update() override;
};