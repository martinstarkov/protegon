#pragma once

#include "State.h"

class JumpState : public State<JumpState> {
public:
	virtual void onEnter() override;
	virtual void onExit() override;
	virtual void update() override;
};