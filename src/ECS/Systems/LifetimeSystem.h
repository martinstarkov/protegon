#pragma once
#include "System.h"

constexpr float LIFE_CHANGE_PER_FRAME = (1.0f / 60.0f);

class LifetimeSystem : public System<LifetimeComponent> {
private:
	using BaseType = System<LifetimeComponent>;
public:
	virtual void update() override {
		std::cout << "Lifetime[" << _entities.size() << "],";
		for (auto& pair : _entities) {
			LifetimeComponent* lifetime = pair.second->getComponent<LifetimeComponent>();
			//CollisionComponent* collision = pair.second->getComponent<CollisionComponent>(); // TODO in the future
			//if (collision->_bottom) { lifetime->_isDying = true; }
			if (lifetime->_isDying) {
				if (lifetime->_lifetime - LIFE_CHANGE_PER_FRAME >= 0) {
					lifetime->_lifetime -= LIFE_CHANGE_PER_FRAME;
				} else {
					lifetime->_lifetime = 0.0f;
				}
			}
			if (!lifetime->_lifetime) {
				pair.second->destroy();
			}
		}
	}
};