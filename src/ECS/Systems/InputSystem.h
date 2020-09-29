#pragma once

#include "System.h"

#include <SDL.h>

// TODO: Add key specificity to InputComponent

class InputSystem : public ecs::System<InputComponent> {
public:
	virtual void Update() override final {
		s = SDL_GetKeyboardState(NULL);
		for (auto [entity, input] : entities) {
			if (entity.HasComponent<PlayerController>()) {
				auto& playerController = entity.GetComponent<PlayerController>();
				// Technically player could be without a RigidBodyComponent ;)
				if (entity.HasComponent<RigidBodyComponent>()) {
					auto& rigidBodyC = entity.GetComponent<RigidBodyComponent>();
					physicsInputs(entity, rigidBodyC.rigidBody, playerController);
				}
				auto all_entities = GetManager().GetEntities();
				if (s[SDL_SCANCODE_R]) {
					for (auto& entity2 : all_entities) {
						if (entity2.HasComponent<TransformComponent>()) {
							entity2.GetComponent<TransformComponent>().ResetPosition();
						}
						if (entity2.HasComponent<RigidBodyComponent>()) {
							auto& rc = entity2.GetComponent<RigidBodyComponent>();
							rc.rigidBody.velocity = Vec2D{};
							rc.rigidBody.acceleration = Vec2D{};
						}
					}
				}
				if (s[SDL_SCANCODE_B]) {
					for (auto& entity2 : all_entities) {
						if (entity2.HasComponent<RigidBodyComponent>()) {
							auto& rc = entity2.GetComponent<RigidBodyComponent>();
							rc.rigidBody.velocity = Vec2D(rand() % 40 - 20, rand() % 40 - 20);
						}
					}
				}
				// clear all entities except player
				if (s[SDL_SCANCODE_C]) {
					for (auto& entity2 : all_entities) {
						if (entity2 != entity) {
							entity2.Destroy();
						}
					}
				}
				if (s[SDL_SCANCODE_H]) {
					//Serialization::serialize("resources/player.json", e);
				}
			}
		}
	}
	// player pressing motions keys
	void physicsInputs(ecs::Entity entity, RigidBody& rigidBody, PlayerController& player) {
		rigidBody.acceleration = Vec2D{};
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