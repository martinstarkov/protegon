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
			MotionComponent* motion = e.getComponent<MotionComponent>();
			if (motion) {
				if (s[SDL_SCANCODE_W] && !s[SDL_SCANCODE_S]) {
					motion->_velocity.y += -player->_speed.y;
				}
				if (s[SDL_SCANCODE_S] && !s[SDL_SCANCODE_W]) {
					motion->_velocity.y += player->_speed.y;
				}
				if (s[SDL_SCANCODE_A] && !s[SDL_SCANCODE_D]) {
					motion->_velocity.x += -player->_speed.x;
				}
				if (s[SDL_SCANCODE_D] && !s[SDL_SCANCODE_A]) {
					motion->_velocity.x += player->_speed.x;
				}
			}
		}
	}
	void motion(Entity& e) {
	}
private:
	const Uint8* s = nullptr;
};