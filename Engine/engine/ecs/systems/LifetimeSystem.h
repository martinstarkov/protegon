#pragma once

#include "System.h"

// TODO: Make it clearer how this system acts as a component (health vs bullet lifetime vs block lifetime)

class LifetimeSystem : public ecs::System<LifetimeComponent> {
public:
	virtual void Update() override final {
		for (auto& [entity, life] : entities) {
			// TODO: In the future, for falling platforms, decrease lifetime if there is a bottom collision
			//CollisionComponent* collision = pair.second->getComponent<CollisionComponent>();
			//if (collision->bottom) { lifetime->isDying = true; }
			if (life.is_dying) {
				if (life.lifetime - SECOND_CHANGE_PER_FRAME >= 0.0) {
					life.lifetime -= SECOND_CHANGE_PER_FRAME;
				} else {
					life.lifetime = 0.0;
				}
			}
			if (!life.lifetime) {
				entity.Destroy();
			}
		}
	}
};