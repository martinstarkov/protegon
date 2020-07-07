#pragma once

#include "State.h"

class JumpState : public State<JumpState> {
public:
	virtual void update() override;
};