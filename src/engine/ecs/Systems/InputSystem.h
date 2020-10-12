#pragma once

#include "System.h"

#include <engine/event/InputHandler.h>

// TODO: Add key specificity to InputComponent

class InputSystem : public ecs::System<InputComponent> {
public:
	virtual void Update() override final {
		using namespace engine;
		for (auto [entity, input] : entities) {
			if (entity.HasComponent<PlayerController>()) {
				auto& player = entity.GetComponent<PlayerController>();
				// Technically player could be without a RigidBodyComponent ;)
				if (entity.HasComponent<RigidBodyComponent>()) {
					auto& rigid_body = entity.GetComponent<RigidBodyComponent>().rigid_body;
					PhysicsInputs(entity, rigid_body, player);
				}
				auto all_entities = GetManager().GetEntities();
				if (InputHandler::KeyPressed(Key::R)) {
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
				if (InputHandler::KeyPressed(Key::B)) {
					for (auto& entity2 : all_entities) {
						if (entity2.HasComponent<RigidBodyComponent>()) {
							auto& rb = entity2.GetComponent<RigidBodyComponent>().rigid_body;
							rb.velocity = V2_double::Random(-20.0, 20.0, -20.0, 20.0);
						}
					}
				}
				// clear all entities except player
				if (InputHandler::KeyPressed(Key::C)) {
					for (auto& entity2 : all_entities) {
						if (entity2 != entity) {
							entity2.Destroy();
						}
					}
				}
			}
		}
	}
	// player pressing motions keys
	void PhysicsInputs(ecs::Entity entity, RigidBody& rigidBody, PlayerController& player) {
		using namespace engine;
		rigidBody.acceleration = { 0.0, 0.0 };
		if ((InputHandler::KeyPressed(Key::A) && InputHandler::KeyPressed(Key::D)) || (InputHandler::KeyReleased(Key::A) && InputHandler::KeyReleased(Key::D))) { // both horizontal keys pressed or neither -> stop
			rigidBody.acceleration.x = 0.0;
		} else if (InputHandler::KeyPressed(Key::A) && InputHandler::KeyReleased(Key::D)) { // left
			rigidBody.acceleration.x = -player.input_acceleration.x;
		} else if (InputHandler::KeyPressed(Key::D) && InputHandler::KeyReleased(Key::A)) { // right
			rigidBody.acceleration.x = player.input_acceleration.x;
		}
		if ((InputHandler::KeyPressed(Key::W) && InputHandler::KeyPressed(Key::S)) || (InputHandler::KeyReleased(Key::W) && InputHandler::KeyReleased(Key::S))) { // both vertical keys pressed or neither -> stop (change for gravity)
			rigidBody.acceleration.y = 0.0;
		} else if (InputHandler::KeyPressed(Key::W) && InputHandler::KeyReleased(Key::S)) { // up
			rigidBody.acceleration.y = -player.input_acceleration.y;
		} else if (InputHandler::KeyPressed(Key::S) && InputHandler::KeyReleased(Key::W)) { // down
			rigidBody.acceleration.y = player.input_acceleration.y;
		}
	}
};