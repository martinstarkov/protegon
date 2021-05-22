#pragma once

#include "ecs/ECS.h"

#include "ecs/components/LifetimeComponent.h"

//#include "core/Engine.h"
//
//// TODO: Make it clearer how this system acts as a component (health vs bullet lifetime vs block lifetime)
//
//namespace engine {
//
//class LifetimeSystem : public ecs::System<LifetimeComponent> {
//public:
//	virtual void Update() override final {
//		for (auto& [entity, life] : entities) {
//			// TODO: In the future, for falling platforms, decrease lifetime if there is a bottom collision
//			//CollisionComponent* collision = pair.second->getComponent<CollisionComponent>();
//			//if (collision->bottom) { lifetime->isDying = true; }
//			if (life.is_dying) {
//				if (life.lifetime * Engine::GetFPS() >= 1.0) {
//					life.lifetime -= engine::Engine::GetInverseFPS();
//				} else {
//					life.lifetime = 0.0;
//				}
//			}
//			if (life.lifetime == 0.0) {
//				entity.Destroy();
//			}
//		}
//		GetManager().Refresh();
//	}
//};
//
//} // namespace engine