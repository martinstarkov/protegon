#pragma once

#include "System.h"

class StateMachineSystem : public ecs::System<StateMachineComponent> {
public:
	virtual void Update() override final {
		for (auto& [entity, sm] : entities) {
			for (const auto& pair : sm.state_machines) {
				pair.second->Update();
				//LOG("Current state: " << pair.second->GetState());
			}
		}
	}
};