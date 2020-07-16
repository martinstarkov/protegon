#pragma once

#include "State.h"

class RunState : public State<RunState> {
	virtual void onEntry() override final;
	virtual void update() override final;
};