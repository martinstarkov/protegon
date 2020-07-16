#pragma once

#include "State.h"

class WalkState : public State<WalkState> {
public:
	virtual void onEntry() override final;
	virtual void update() override final;
};

