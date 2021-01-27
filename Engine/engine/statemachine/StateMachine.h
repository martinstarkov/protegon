#pragma once

#include <cstdlib> // std::size_t
#include <memory> // std::make_shared, std::shared_ptr
#include <unordered_map> // std::unordered_map
#include <string> // std::string // TEMPORARY

#include "BaseStateMachine.h"
#include "states/BaseState.h"

#include "utils/math/Hasher.h"

namespace engine {

class StateMachine : public BaseStateMachine {
public:
	StateMachine() = default;
	virtual ~StateMachine() = default;
	virtual void Init(ecs::Entity parent_entity) override final {
		for (const auto& pair : states_) {
			pair.second->SetParentStateMachine(this);
			pair.second->SetParentEntity(parent_entity);
		}
		current_state_->OnEntry();
	}
	virtual void SetState(const char* name) override final {
		auto key = Hasher::HashCString(name);
		auto it = states_.find(key);
		assert(it != states_.end() && "Cannot SetState to nonexistent state");
		// Check that state change isn't to the same state as is currently active.
		if (it->second != current_state_) {
			// Set states to new ones.
			previous_state_ = current_state_;
			current_state_ = it->second;
			// Enter and exit current / previous states.
			previous_state_->OnExit();
			current_state_->OnEntry();
		}
	}
	// Called once per update cycle, update's the state machine's current state.
	virtual void Update() override final {
		assert(current_state_ != nullptr && "Current state undefined");
		current_state_->Update();
	}
	// TEMPORARY : Hacked together way of debugging current state.
	virtual std::string GetState() {
		std::size_t current_key = 0;
		for (const auto& pair : states_) {
			if (current_state_ == pair.second) {
				current_key = pair.first;
			}
		}
		auto it = state_names_.find(current_key);
		assert(it != state_names_.end() && "No current state found");
		return it->second;
	}
protected:
	virtual void AddState(const char* name, std::shared_ptr<BaseState> state) override final {
		// TODO: In the future replace hasher with integer serialized ids.
		auto key = Hasher::HashCString(name);
		assert(states_.find(key) == states_.end() && "Cannot add duplicate state into state machine");
		// Set first state as default initial / previous state.
		if (states_.size() == 0) {
			current_state_ = state;
			previous_state_ = current_state_;
		}
		states_.emplace(key, std::move(state));
		state_names_.emplace(key, name);
	}
private:
	std::unordered_map<std::size_t, std::shared_ptr<BaseState>> states_;
	// TEMPORARY
	std::unordered_map<std::size_t, std::string> state_names_;
	std::shared_ptr<BaseState> current_state_ = nullptr;
	std::shared_ptr<BaseState> previous_state_ = nullptr;
};

} // namespace engine