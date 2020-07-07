#pragma once

#include "BaseState.h"
#include <typeinfo>

template <class StateType>
class State : public BaseState {
public:
	State() : _sm(nullptr) {
		_id = static_cast<StateID>(typeid(StateType).hash_code());
	}
	virtual void onExit() override {}
	virtual void onEntry() override {}
	virtual void update() override {}
	virtual void setParentStateMachine(BaseStateMachine* parentStateMachine) override final {
		_sm = parentStateMachine;
	}
	virtual StateID getStateID() override final { return _id; }
protected:
	BaseStateMachine* _sm;
private:
	StateID _id;
};
