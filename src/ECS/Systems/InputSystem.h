#pragma once

#include "System.h"

#include "SDL.h"

// TODO: Add key specificity to InputComponent

class InputSystem : public System<InputComponent> {
public:
	virtual void update() override final {
		s = SDL_GetKeyboardState(NULL);
		for (auto& id : entities) {
			Entity e = Entity(id, manager);
			auto [input] = getComponents(id);
			auto playerController = e.getComponent<PlayerController>();
			if (playerController) {
				auto rigidBodyC = e.getComponent<RigidBodyComponent>();
				// Technically player could be without a RigidBodyComponent ;)
				if (rigidBodyC) {
					physicsInputs(e, rigidBodyC->rigidBody, *playerController);
				}
				if (s[SDL_SCANCODE_R]) {
					for (auto& id : manager->getEntities()) {
						auto [oTransform, oRigidBodyC] = getComponents<TransformComponent, RigidBodyComponent>(id);
						if (oTransform) {
							oTransform->position = oTransform->originalPosition;
						}
						if (oRigidBodyC) {
							oRigidBodyC->rigidBody.velocity = Vec2D();
							oRigidBodyC->rigidBody.acceleration = Vec2D();
						}
					}
				}
				if (s[SDL_SCANCODE_B]) {
					for (auto& id : manager->getEntities()) {
						auto [oRigidBodyC] = getComponents<RigidBodyComponent>(id);
						if (oRigidBodyC) {
							oRigidBodyC->rigidBody.velocity = Vec2D(rand() % 40 - 20, rand() % 40 - 20);
						}
					}
				}
				// clear all entities except player
				if (s[SDL_SCANCODE_C]) {
					for (auto& rId : manager->getEntities({ id })) {
						manager->destroyEntity(rId);
					}
					manager->refreshDeleted();
				}
				if (s[SDL_SCANCODE_H]) {
					//Serialization::serialize("resources/player.json", e);
				}
			}
		}
	}
	// player pressing motions keys
	void physicsInputs(Entity entity, RigidBody& rigidBody, PlayerController& player) {
		rigidBody.acceleration = Vec2D();
		if ((s[SDL_SCANCODE_A] && s[SDL_SCANCODE_D]) || (!s[SDL_SCANCODE_A] && !s[SDL_SCANCODE_D])) { // both horizontal keys pressed or neither -> stop
			rigidBody.acceleration.x = 0.0;
		} else if (s[SDL_SCANCODE_A] && !s[SDL_SCANCODE_D]) { // left
			rigidBody.acceleration.x = -player.inputAcceleration.x;
		} else if (s[SDL_SCANCODE_D] && !s[SDL_SCANCODE_A]) { // right
			rigidBody.acceleration.x = player.inputAcceleration.x;
		}
		if ((s[SDL_SCANCODE_W] && s[SDL_SCANCODE_S]) || (!s[SDL_SCANCODE_W] && !s[SDL_SCANCODE_S])) { // both vertical keys pressed or neither -> stop (change for gravity)
			rigidBody.acceleration.y = 0.0;
		} else if (s[SDL_SCANCODE_W] && !s[SDL_SCANCODE_S]) { // up
			rigidBody.acceleration.y = -player.inputAcceleration.y;
		} else if (s[SDL_SCANCODE_S] && !s[SDL_SCANCODE_W]) { // down
			rigidBody.acceleration.y = player.inputAcceleration.y;
		}
	}
private:
	const Uint8* s = nullptr;
};