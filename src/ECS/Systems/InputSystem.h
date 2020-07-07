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
		PlayerController* player = e.getComponent<PlayerController>();
		if (player) {
			StateMachineComponent* sm = e.getComponent<StateMachineComponent>();
			if (sm) {
				BaseStateMachine* walkStateMachine = sm->_stateMachines[0];
				StateID walkStateID = walkStateMachine->getCurrentState()->getStateID();
				if (walkStateID == typeid(IdleState).hash_code() || walkStateID == typeid(WalkState).hash_code()) {
					player->_speed.x = player->_originalSpeed.x;
				} else if (walkStateID == typeid(RunState).hash_code()) {
					player->_speed.x *= 1.8f;
				}
			}
			MotionComponent* motion = e.getComponent<MotionComponent>();
			if (motion) {
				// movement occurs
				if (s[SDL_SCANCODE_W] || s[SDL_SCANCODE_A] || s[SDL_SCANCODE_S] || s[SDL_SCANCODE_D]) {
					if (s[SDL_SCANCODE_W] && !s[SDL_SCANCODE_S]) { // jump
						if (sm) {
							sm->_stateMachines[1]->setCurrentState(std::make_unique<JumpState>());
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
								sm->_stateMachines[0]->setCurrentState(std::make_unique<WalkState>());
							}
						}
					}
					if (s[SDL_SCANCODE_D] && !s[SDL_SCANCODE_A]) { // walk right
						motion->_velocity.x += player->_speed.x;
						if (sm) {
							if (!sm->_stateMachines[0]->isState(typeid(RunState).hash_code())) {
								sm->_stateMachines[0]->setCurrentState(std::make_unique<WalkState>());
							}
						}
					}
					if (s[SDL_SCANCODE_A] && s[SDL_SCANCODE_D]) {
						sm->_stateMachines[0]->setCurrentState(std::make_unique<IdleState>());
					}
				} else { // player stops
					if (sm) {
						sm->_stateMachines[0]->setCurrentState(std::make_unique<IdleState>());
					}
				}
			}
		}
	}
	void motion(Entity& e) {
	}
private:
	const Uint8* s = nullptr;
};