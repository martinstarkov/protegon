#pragma once

#include "System.h"

// TODO: Make it clearer how this system acts as a component (health vs bullet lifetime vs block lifetime)

class LifetimeSystem : public System<LifetimeComponent> {
public:
	virtual void update() override final {
		for (auto& id : entities) {
			Entity e = Entity(id, manager);
			LifetimeComponent* lifetime = e.getComponent<LifetimeComponent>();
			//CollisionComponent* collision = pair.second->getComponent<CollisionComponent>(); // TODO in the future
			//if (collision->bottom) { lifetime->isDying = true; }
			if (lifetime->isDying) {
				if (lifetime->lifetime - SECOND_CHANGE_PER_FRAME >= 0.0) {
					lifetime->lifetime -= SECOND_CHANGE_PER_FRAME;
				} else {
					lifetime->lifetime = 0.0;
				}
			}
			if (!lifetime->lifetime) {
				e.destroy();
			}
		}
	}
};