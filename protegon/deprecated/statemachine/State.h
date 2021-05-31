#pragma once

#include "statemachine/BaseState.h"
#include "statemachine/BaseStateMachine.h"

#include "ecs/Components.h" // Each state should have access to all components

namespace engine {

class State : public BaseState {
public:
	State() = default;
	virtual ~State() = default;
	virtual void OnExit() override {}
	virtual void OnEntry() override {}
	virtual void Update() override {}
	virtual void SetParentEntity(const ecs::Entity& entity) override final {
		parent_entity = entity;
	}
	virtual void SetParentStateMachine(BaseStateMachine* state_machine) override final {
		parent_state_machine = state_machine;
	}
protected:
	ecs::Entity parent_entity;
	BaseStateMachine* parent_state_machine = nullptr;
};

} // namespace engine