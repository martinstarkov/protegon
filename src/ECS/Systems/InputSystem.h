#pragma once

#include "System.h"

#include "SDL.h"

// TODO: Add key specificity to InputComponent

class InputSystem : public System<InputComponent> {
public:
	virtual void update() override {
		s = SDL_GetKeyboardState(NULL);
		for (auto& entityID : _entities) {
			Entity& e = getEntity(entityID);
			keyStateCheck(e);
		}
	}
	void keyStateCheck(Entity& e) {
		if (s[SDL_SCANCODE_Q]) {
			e.addComponents(Serialization::deserialize<SpriteComponent>("resources/sprite1.json"));
		}
		if (s[SDL_SCANCODE_E]) {
			e.addComponents(Serialization::deserialize<SpriteComponent>("resources/sprite2.json"));
		}
		if (s[SDL_SCANCODE_R]) {
			Serialization::reserialize("resources/animation.json", *e.getComponent<AnimationComponent>());
		}
		if (s[SDL_SCANCODE_T]) {
			Serialization::reserialize("resources/sprite1.json", *e.getComponent<SpriteComponent>());
		}
		if (s[SDL_SCANCODE_Y]) {
			Serialization::reserialize("resources/sprite2.json", *e.getComponent<SpriteComponent>());
		}
		if (s[SDL_SCANCODE_C]) {
			e.removeComponents<SpriteComponent>();
		}
		motion(e);
	}
	void motion(Entity& e) {
		PlayerController* player = e.getComponent<PlayerController>();
		if (player) {
			MotionComponent* motion = e.getComponent<MotionComponent>();
			if (motion) {
				StateMachineComponent* sm = e.getComponent<StateMachineComponent>();
				if (sm) {
					BaseStateMachine* walkStateMachine = sm->_stateMachines[0];
					StateID walkStateID = walkStateMachine->getCurrentState()->getStateID();
					if (walkStateID == typeid(IdleState).hash_code() || walkStateID == typeid(WalkState).hash_code()) {
						player->_speed.x = player->_originalSpeed.x;
					} else if (walkStateID == typeid(RunState).hash_code()) {
						player->_speed.x = player->_originalSpeed.x * 2.0f;
					}
				}
				// movement occurs
				if (s[SDL_SCANCODE_W] || s[SDL_SCANCODE_A] || s[SDL_SCANCODE_S] || s[SDL_SCANCODE_D]) {
					if (s[SDL_SCANCODE_W] && !s[SDL_SCANCODE_S]) { // jump
						if (sm) {
							sm->_stateMachines[1]->setCurrentState(std::move(std::make_unique<JumpState>()));
						}
						motion->_velocity.y += -player->_speed.y;
					}
					if (s[SDL_SCANCODE_S] && !s[SDL_SCANCODE_W]) { // press down
						motion->_velocity.y += player->_speed.y;
					}
					if (s[SDL_SCANCODE_A] && !s[SDL_SCANCODE_D]) { // walk left
						motion->_velocity.x += -player->_speed.x;
						if (sm) {
							if (!sm->_stateMachines[0]->isState(typeid(RunState).hash_code())) {
								sm->_stateMachines[0]->setCurrentState(std::move(std::make_unique<WalkState>()));
							}
						}
					}
					if (s[SDL_SCANCODE_D] && !s[SDL_SCANCODE_A]) { // walk right
						motion->_velocity.x += player->_speed.x;
						if (sm) {
							if (!sm->_stateMachines[0]->isState(typeid(RunState).hash_code())) {
								sm->_stateMachines[0]->setCurrentState(std::move(std::make_unique<WalkState>()));
							}
						}
					}
					if (s[SDL_SCANCODE_A] && s[SDL_SCANCODE_D]) {
						if (sm) {
							sm->_stateMachines[0]->setCurrentState(std::move(std::make_unique<IdleState>()));
						}
					}
				} else { // player stops
					if (sm) {
						sm->_stateMachines[0]->setCurrentState(std::move(std::make_unique<IdleState>()));
					}
				}
			}
		}
	}
private:
	const Uint8* s = nullptr;
};