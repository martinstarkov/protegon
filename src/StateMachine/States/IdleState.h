#pragma once

#include "State.h"

class IdleState : public State<IdleState> {
public:
	virtual void onEnter() override;
	virtual void onExit() override;
	virtual void update() override;
};

