#pragma once

#include "System.h"

class DeadStateSystem : public System<StateComponent, DeadState> {
public:
	virtual void update() override {
		//LOG("DeadStateEntities[" << _entities.size() << "],");
		for (auto& entityID : _entities) {
			Entity& e = getEntity(entityID);
			StateComponent* sm = e.getComponent<StateComponent>();
			int& countdown = sm->_sm.getState<DeadState>()._countdown;
			if (!countdown) {
				e.destroy();
			}
			countdown -= 1;
		}
	}
};