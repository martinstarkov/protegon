#pragma once

#include "ecs/System.h"

#include "StateMachineComponent.h"

class StateMachineSystem : public ecs::System<StateMachineComponent> {
public:
	virtual void Update() override final {
		for (auto& [entity, sm] : entities) {
			auto end{ sm.state_machines.end() };
			auto it{ sm.state_machines.begin() };
			while (it != end) {
				it->second->Update();
				++it;
			}
			/*for (auto& pair : sm.state_machines) {
				pair.second->Update();
			}*/
		}
		GetManager().Refresh();
	}
};