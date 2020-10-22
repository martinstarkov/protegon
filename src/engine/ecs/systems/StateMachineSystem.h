// TODO: Fix state machine system.

//#pragma once
//#include "System.h"
//
//#include <StateMachine/StateMachines.h>
//
//class StateMachineSystem : public ecs::System<StateMachineComponent> {
//public:
//	virtual void Update() override final {
//		for (auto& [entity, sm] : entities) {
//			for (const auto& pair : sm.stateMachines) {
//				pair.second->update();
//			}
//		}
//	}
//};