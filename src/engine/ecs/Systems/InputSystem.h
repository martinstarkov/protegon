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
				auto& player = entity.GetComponent<PlayerController>();
				// Technically player could be without a RigidBodyComponent ;)
				if (entity.HasComponent<RigidBodyComponent>()) {
					auto& rigid_body = entity.GetComponent<RigidBodyComponent>().rigid_body;
					PhysicsInputs(entity, rigid_body, player);
				}
				auto all_entities = GetManager().GetEntities();
				if (s[SDL_SCANCODE_R]) {
					for (auto& entity2 : all_entities) {
						if (entity2.HasComponent<TransformComponent>()) {
							entity2.GetComponent<TransformComponent>().ResetPosition();
						}
						if (entity2.HasComponent<RigidBodyComponent>()) {
							auto& rb = entity2.GetComponent<RigidBodyComponent>().rigid_body;
							rb.velocity = { 0.0, 0.0 };
							rb.acceleration = { 0.0, 0.0 };
						}
					}
				}
				if (s[SDL_SCANCODE_B]) {
					for (auto& entity2 : all_entities) {
						if (entity2.HasComponent<RigidBodyComponent>()) {
							auto& rb = entity2.GetComponent<RigidBodyComponent>().rigid_body;
							rb.velocity = V2_double::Random(-20.0, 20.0, -20.0, 20.0);
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
					SDL_Delay(400);
					//Serialization::serialize("resources/player.json", e);
				}
				if (s[SDL_SCANCODE_J]) {
					SDL_Delay(3000);
					//Serialization::serialize("resources/player.json", e);
				}
			}
		}
	}
	// player pressing motions keys
	void PhysicsInputs(ecs::Entity entity, RigidBody& rigidBody, PlayerController& player) {
		rigidBody.acceleration = { 0.0, 0.0 };
		if ((s[SDL_SCANCODE_A] && s[SDL_SCANCODE_D]) || (!s[SDL_SCANCODE_A] && !s[SDL_SCANCODE_D])) { // both horizontal keys pressed or neither -> stop
			rigidBody.acceleration.x = 0.0;
		} else if (s[SDL_SCANCODE_A] && !s[SDL_SCANCODE_D]) { // left
			rigidBody.acceleration.x = -player.input_acceleration.x;
		} else if (s[SDL_SCANCODE_D] && !s[SDL_SCANCODE_A]) { // right
			rigidBody.acceleration.x = player.input_acceleration.x;
		}
		if ((s[SDL_SCANCODE_W] && s[SDL_SCANCODE_S]) || (!s[SDL_SCANCODE_W] && !s[SDL_SCANCODE_S])) { // both vertical keys pressed or neither -> stop (change for gravity)
			rigidBody.acceleration.y = 0.0;
		} else if (s[SDL_SCANCODE_W] && !s[SDL_SCANCODE_S]) { // up
			rigidBody.acceleration.y = -player.input_acceleration.y;
		} else if (s[SDL_SCANCODE_S] && !s[SDL_SCANCODE_W]) { // down
			rigidBody.acceleration.y = player.input_acceleration.y;
		}
	}
private:
	const Uint8* s = nullptr;
};