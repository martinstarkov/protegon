#include "InputHandler.h"
#include "Game.h"
#include "Player.h"

InputHandler* InputHandler::instance = nullptr;

InputHandler::InputHandler() {}

#define ACCELERATION_SCALE 2.0f
#define JUMP 9.0f
#define RESET_POS Vec2D(128, 128)

void InputHandler::keyStateChange() {
	Player* player = Player::getInstance();
	const Uint8* states = SDL_GetKeyboardState(NULL);
	if (states[SDL_SCANCODE_A]) {
		player->setAcceleration(Vec2D(-1 * ACCELERATION_SCALE, player->getAcceleration().y));
	}
	if (states[SDL_SCANCODE_D]) {
		player->setAcceleration(Vec2D(1 * ACCELERATION_SCALE, player->getAcceleration().y));
	}
	if (states[SDL_SCANCODE_W]) {
		player->setAcceleration(Vec2D(player->getAcceleration().x, -1 * JUMP));
	} 
	if (states[SDL_SCANCODE_S]) {
		player->setAcceleration(Vec2D(player->getAcceleration().x, 1 * ACCELERATION_SCALE));
	}
	if (!states[SDL_SCANCODE_A] && !states[SDL_SCANCODE_D]) {
		player->setAcceleration(Vec2D(0, player->getAcceleration().y));
	}
	if (!states[SDL_SCANCODE_W] && !states[SDL_SCANCODE_S]) {
		player->setAcceleration(Vec2D(player->getAcceleration().x, 0));
	}
	if (states[SDL_SCANCODE_R]) {
		player->setPosition(RESET_POS);
	}
}

void InputHandler::update() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
				Game::getInstance()->quit();
				break;
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				keyStateChange();
				break;
			default:
				break;
		}
	}
}
