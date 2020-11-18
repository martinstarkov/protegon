#pragma once

#include <engine/Include.h>

class MitosisSystem : public ecs::System<PlayerController, TransformComponent, CollisionComponent, RigidBodyComponent> {
	virtual void Update() override final {
		for (auto [entity, player, transform, collider, rigid_body] : entities) {
			if (engine::InputHandler::KeyDown(Key::M)) {
				CreatePlayer({ transform.position.x, transform.position.y - 1 }, { collider.collider.size.x, collider.collider.size.y / 2 }, GetManager());
				CreatePlayer({ transform.position.x, transform.position.y + collider.collider.size.y - 1 }, { collider.collider.size.x, collider.collider.size.y / 2 }, GetManager());
				entity.Destroy();
			}
		}
	}
};