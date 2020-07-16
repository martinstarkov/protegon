#pragma once

#include "BaseState.h"

template <class StateType>
class State : public BaseState {
public:
	State() : parentStateMachine(nullptr), parentEntity(nullptr), _name(typeid(StateType).name()) {}
	virtual void onExit() override {}
	virtual void onEntry() override {}
	virtual void update() override {}
	virtual void setName(StateName name) override final {
		_name = name;
	}
	virtual StateName getName() override final {
		return _name;
	}
	virtual void setParentEntity(Entity* newParentEntity) override final {
		parentEntity = newParentEntity;
	}
	virtual void setParentStateMachine(BaseStateMachine* newParentStateMachine) override final {
		parentStateMachine = newParentStateMachine;
	}
protected:
	Entity* parentEntity;
	BaseStateMachine* parentStateMachine;
private:
	StateName _name;
};
