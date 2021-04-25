#pragma once

#include "ecs/ECS.h"

#include "event/InputHandler.h"

#include "ecs/components/InputComponent.h"
#include "ecs/components/RigidBodyComponent.h"

// TODO: Add key specificity to InputComponent

namespace engine {

class InputSystem : public ecs::UESystem<InputComponent, RigidBodyComponent> {
public:
	virtual void Update() override final {
		auto [entity, input, rigid_body] = GetEntityAndComponents();
		PhysicsInputs(rigid_body.body, { 0.1, 0.1 });
	}
	// player pressing motions keys
	void PhysicsInputs(RigidBody& rigidBody, const V2_double& input_acceleration) {
		rigidBody.acceleration = { 0.0, 0.0 };
		if ((InputHandler::KeyPressed(Key::A) && InputHandler::KeyPressed(Key::D)) || (InputHandler::KeyReleased(Key::A) && InputHandler::KeyReleased(Key::D))) { // both horizontal keys pressed or neither -> stop
			rigidBody.acceleration.x = 0.0;
		} else if (InputHandler::KeyPressed(Key::A) && InputHandler::KeyReleased(Key::D)) { // left
			rigidBody.acceleration.x = -input_acceleration.x;
		} else if (InputHandler::KeyPressed(Key::D) && InputHandler::KeyReleased(Key::A)) { // right
			rigidBody.acceleration.x = input_acceleration.x;
		}
		if ((InputHandler::KeyPressed(Key::W) && InputHandler::KeyPressed(Key::S)) || (InputHandler::KeyReleased(Key::W) && InputHandler::KeyReleased(Key::S))) { // both vertical keys pressed or neither -> stop (change for gravity)
			rigidBody.acceleration.y = 0.0;
		} else if (InputHandler::KeyPressed(Key::W) && InputHandler::KeyReleased(Key::S)) { // up
			rigidBody.acceleration.y = -input_acceleration.y;
		} else if (InputHandler::KeyPressed(Key::S) && InputHandler::KeyReleased(Key::W)) { // down
			rigidBody.acceleration.y = input_acceleration.y;
		}
	}
};

} // namespace engine;