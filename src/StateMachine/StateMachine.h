#pragma once

#include "BaseStateMachine.h"
#include "States/BaseState.h"

class StateMachine : public BaseStateMachine {
public:
	StateMachine(StateName initialState);
	virtual void update() override final;
	virtual StateMachineName getName() override final;
	virtual void setName(StateMachineName name) override final;
	virtual BaseState* getCurrentState() override final;
	virtual void setCurrentState(StateName state) override final;
	virtual bool inState(StateName name) override final;
protected:
	virtual void init(Entity handle) override final;
	StateMap states;
private:
	StateMachineName _name;
	StateName _currentState;
	StateName _previousState;
};