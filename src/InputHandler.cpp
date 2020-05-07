#include "InputHandler.h"
#include "Game.h"
#include "Player.h"

InputHandler* InputHandler::instance = nullptr;

Player* player;

InputHandler::InputHandler() {
	player = Player::getInstance();
}

#define MOVEMENT_ACCELERATION 1.0f
#define JUMPING_ACCELERATION 3.8f
#define RESET_POSITION Vec2D(232, 128)

void InputHandler::keyStateChange() {
	const Uint8* states = SDL_GetKeyboardState(NULL);
	if (states[SDL_SCANCODE_A]) {
		player->setAcceleration(Vec2D(-MOVEMENT_ACCELERATION, player->getAcceleration().y));
	}
	if (states[SDL_SCANCODE_D]) {
		player->setAcceleration(Vec2D(MOVEMENT_ACCELERATION, player->getAcceleration().y));
	}
	if (states[SDL_SCANCODE_W]) {
		if (!player->jumping) {
			player->jumping = true;
			player->setAcceleration(Vec2D(player->getAcceleration().x, -JUMPING_ACCELERATION));
		}
	} 
	if (states[SDL_SCANCODE_S]) {
		player->setAcceleration(Vec2D(player->getAcceleration().x, MOVEMENT_ACCELERATION / 5));
	}
	if (!states[SDL_SCANCODE_A] && !states[SDL_SCANCODE_D]) {
		player->setAcceleration(Vec2D(0.0f, player->getAcceleration().y));
	}
	if (!states[SDL_SCANCODE_W] && !states[SDL_SCANCODE_S]) {
		// do nothing
	}
	if (states[SDL_SCANCODE_R]) {
		player->setPosition(RESET_POSITION);
	}
}

void InputHandler::update() {
	SDL_Event event;
	keyStateChange();
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
				Game::getInstance()->quit();
				break;
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				break;
			default:
				break;
		}
	}
}
