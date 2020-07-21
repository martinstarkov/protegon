#pragma once

#include "../Types.h"

class EntityHandle;
class BaseStateMachine;

class BaseState {
public:
	BaseState() = default;
	BaseState(const BaseState&) = delete;
	BaseState(BaseState&&) = delete;
	virtual void onEntry() = 0;
	virtual void onExit() = 0;
	virtual void update() = 0;
	virtual StateName getName() = 0;
	virtual void setName(StateName name) = 0;
	virtual void setHandle(EntityHandle handle) = 0;
	virtual void setParentStateMachine(BaseStateMachine* parentStateMachine) = 0;
};

