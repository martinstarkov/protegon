#include "LifetimeSystem.h"

#include "../Components/LifetimeComponent.h"

void LifetimeSystem::update() {
	for (auto& id : entities) {
		EntityHandle e = EntityHandle(id, manager);
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
