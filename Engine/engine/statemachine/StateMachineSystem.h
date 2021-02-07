#pragma once

#include "ecs/System.h"

#include "StateMachineComponent.h"

class StateMachineSystem : public ecs::System<StateMachineComponent> {
public:
	virtual void Update() override final {
		for (auto& [entity, sm] : entities) {
			if (entity.IsAlive()) {
				auto end = sm.state_machines.end();
				auto it = sm.state_machines.begin();
				while (it != end) {
					if (entity.IsAlive()) {
						it->second->Update();
					} else {
						break;
					}
					if (entity.IsAlive()) {
						++it;
					} else {
						break;
					}
				}
			}
			/*for (auto& pair : sm.state_machines) {
				pair.second->Update();
			}*/
		}
	}
};