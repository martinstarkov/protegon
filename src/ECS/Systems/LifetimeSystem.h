#pragma once

#include "System.h"

class LifetimeSystem : public System<LifetimeComponent> {
public:
	virtual void update() override {
		//LOG_("Lifetime[" << _entities.size() << "],");
		for (auto& entityID : _entities) {
			Entity& e = getEntity(entityID);
			LifetimeComponent* lifetime = e.getComponent<LifetimeComponent>();
			//CollisionComponent* collision = pair.second->getComponent<CollisionComponent>(); // TODO in the future
			//if (collision->_bottom) { lifetime->_isDying = true; }
			if (lifetime->_isDying) {
				if (lifetime->_lifetime - SECOND_CHANGE_PER_FRAME >= 0) {
					lifetime->_lifetime -= SECOND_CHANGE_PER_FRAME;
				} else {
					lifetime->_lifetime = 0.0f;
				}
			}
			if (!lifetime->_lifetime) {
				StateComponent* s = e.getComponent<StateComponent>();
				if (s) {
					s->_sm.setState<DeadState>();
				} else {
					e.destroy();
				}
			}
		}
	}
};