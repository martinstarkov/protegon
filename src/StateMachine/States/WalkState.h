#pragma once

#include "State.h"

#define RUN_THRESHOLD 30

class WalkState : public State<WalkState> {
public:
	virtual void onEnter() override;
	virtual void onExit() override;
	virtual void update() override;
private:
	unsigned int _walkTime = 0;
};

