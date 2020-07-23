#pragma once

#include "StateCommon.h"

#include "BaseState.h"
#include "../BaseStateMachine.h"
#include "../../ECS/Entity.h"

template <class StateType>
class State : public BaseState {
public:
	State() : parentStateMachine(nullptr), _name(typeid(StateType).name()) {}
	virtual void onExit() override {}
	virtual void onEntry() override {}
	virtual void update() override {}
	virtual void setName(StateName name) override final {
		_name = name;
	}
	virtual StateName getName() override final {
		return _name;
	}
	virtual void setHandle(Entity handle) override final {
		entity = handle;
	}
	virtual void setParentStateMachine(BaseStateMachine* newParentStateMachine) override final {
		parentStateMachine = newParentStateMachine;
	}
protected:
	Entity entity;
	BaseStateMachine* parentStateMachine;
private:
	StateName _name;
};
