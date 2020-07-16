#pragma once

#include "State.h"

class IdleState : public State<IdleState> {
	virtual void onEntry() override final;
	virtual void update() override final;
};