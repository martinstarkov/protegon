#pragma once

#include "StateMachine.h"

class WalkStateMachine : public StateMachine<WalkStateMachine> {
	virtual void init() override final {
		setCurrentState(new IdleState(this));
	}
};
class JumpStateMachine : public StateMachine<JumpStateMachine> {};