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
			PlayerController* pc = e.getComponent<PlayerController>();
			if (pc) {
				RigidBodyComponent* rb = e.getComponent<RigidBodyComponent>();
				TransformComponent* transform = e.getComponent<TransformComponent>();
				// Technically player could be without a RigidBodyComponent ;)
				if (rb) {
					physicsInputs(e, rb->rigidBody, *pc);
				}
				if (transform) {
					// create block
					if (s[SDL_SCANCODE_B]) {
						Vec2D pos = transform->position + Vec2D(40.0, 0.0);
						manager->createBox(pos);
					}
					if (s[SDL_SCANCODE_R]) {
						transform->position = transform->originalPosition;
					}
				}
				// clear all entities except player
				if (s[SDL_SCANCODE_C]) {
					for (auto& rId : manager->getEntities({ e.getID() })) {
						manager->destroyEntity(rId);
					}
					manager->refreshDeleted();
				}
				if (s[SDL_SCANCODE_H]) {
					Serialization::serialize("resources/player.json", e);
				}
			}
		}
	}
	// player pressing motions keys
	void physicsInputs(Entity& e, RigidBody& rigidBody, PlayerController& player) {
		if (s[SDL_SCANCODE_W] && s[SDL_SCANCODE_S] || !s[SDL_SCANCODE_W] && !s[SDL_SCANCODE_S]) { // both vertical keys pressed or neither -> stop (change for gravity)
			rigidBody.acceleration.y = 0.0;
		} else if (s[SDL_SCANCODE_W] && !s[SDL_SCANCODE_S]) { // up
			rigidBody.velocity.y += -player.inputAcceleration.y;
		} else if (s[SDL_SCANCODE_S] && !s[SDL_SCANCODE_W]) { // down
			rigidBody.velocity.y += player.inputAcceleration.y;
		}
		if (s[SDL_SCANCODE_A] && s[SDL_SCANCODE_D] || !s[SDL_SCANCODE_A] && !s[SDL_SCANCODE_D]) { // both horizontal keys pressed or neither -> stop
			rigidBody.acceleration.x = 0.0;
		} else if (s[SDL_SCANCODE_A] && !s[SDL_SCANCODE_D]) { // left
			rigidBody.velocity.x += -player.inputAcceleration.x;
		} else if (s[SDL_SCANCODE_D] && !s[SDL_SCANCODE_A]) { // right
			rigidBody.velocity.x += player.inputAcceleration.x;
		}
	}
private:
	const Uint8* s = nullptr;
};