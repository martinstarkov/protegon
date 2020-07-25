#pragma once

#include "../Types.h"

class Entity;
class BaseStateMachine;

class BaseState {
public:
	virtual BaseState* clone() const = 0;
	virtual std::unique_ptr<BaseState> uniqueClone() const = 0;
	virtual void onEntry() = 0;
	virtual void onExit() = 0;
	virtual void update() = 0;
	virtual StateName getName() = 0;
	virtual void setName(StateName name) = 0;
	virtual void setHandle(Entity handle) = 0;
	virtual void setParentStateMachine(BaseStateMachine* parentStateMachine) = 0;
};

