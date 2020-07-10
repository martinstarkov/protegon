#pragma once

#include "State.h"

#define RUN_THRESHOLD 30

class WalkState : public State<WalkState> {
public:
	virtual void update() override;
};

