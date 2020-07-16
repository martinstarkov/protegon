#pragma once

#include "../Types.h"

class BaseStateMachine;
class Entity;

class BaseState {
public:
	virtual void onEntry() = 0;
	virtual void onExit() = 0;
	virtual void update() = 0;
	virtual StateName getName() = 0;
	virtual void setName(StateName name) = 0;
	virtual void setParentEntity(Entity* parentEntity) = 0;
	virtual void setParentStateMachine(BaseStateMachine* parentStateMachine) = 0;
};

