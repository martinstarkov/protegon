#pragma once

#include "ecs/System.h"

// TODO: Make it clearer how this system acts as a component (health vs bullet lifetime vs block lifetime)

class LifetimeSystem : public ecs::System<LifetimeComponent> {
public:
	virtual void Update() override final {
		for (auto& [entity, life] : entities) {
			// TODO: In the future, for falling platforms, decrease lifetime if there is a bottom collision
			//CollisionComponent* collision = pair.second->getComponent<CollisionComponent>();
			//if (collision->bottom) { lifetime->isDying = true; }
			if (entity.IsAlive()) {
				if (life.is_dying) {
					if (life.lifetime - engine::Engine::GetInverseFPS() >= 0.0) {
						life.lifetime -= engine::Engine::GetInverseFPS();
					} else {
						life.lifetime = 0.0;
					}
				}
				if (life.lifetime == 0.0) {
					entity.Destroy();
				}
			}
		}
	}
};