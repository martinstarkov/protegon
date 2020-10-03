#pragma once

#include "StateDefines.h"

#include "BaseState.h"
#include "../BaseStateMachine.h"
#include <ECS/Components.h>

template <typename T>
class State : public BaseState {
public:
	State() : parentStateMachine(nullptr), _name(typeid(T).name()) {}
	virtual ~State() = default;
	virtual BaseState* clone() const override final {
		return new T(static_cast<const T&>(*this));
	}
	virtual std::unique_ptr<BaseState> uniqueClone() const override final {
		return std::make_unique<T>(static_cast<const T&>(*this));
	}
	virtual void onExit() override {}
	virtual void onEntry() override {}
	virtual void update() override {}
	virtual void setName(StateName name) override final {
		_name = name;
	}
	virtual StateName getName() override final {
		return _name;
	}
	virtual void setHandle(ecs::Entity handle) override final {
		entity = handle;
	}
	virtual void setParentStateMachine(BaseStateMachine* newParentStateMachine) override final {
		parentStateMachine = newParentStateMachine;
	}
protected:
	ecs::Entity entity;
	BaseStateMachine* parentStateMachine;
private:
	StateName _name;
};
