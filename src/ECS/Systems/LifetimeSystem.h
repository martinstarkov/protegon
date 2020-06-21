#pragma once

#include "System.h"

// TODO: Make it clearer how this system acts as a component (health vs bullet lifetime vs block lifetime)

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
				if (!e.getComponent<InputComponent>()) {
					e.addComponents(InputComponent());
				}
				//e.destroy();
			}
		}
	}
};