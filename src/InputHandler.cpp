#include "InputHandler.h"
#include "Game.h"
#include "Player.h"

InputHandler* InputHandler::instance = nullptr;

Player* player;

InputHandler::InputHandler() {
	player = Player::getInstance();
}

void InputHandler::keyStateCheck() {
	const Uint8* states = SDL_GetKeyboardState(NULL);
	if (states[SDL_SCANCODE_A]) {
		player->move(LEFT);
	}
	if (states[SDL_SCANCODE_D]) {
		player->move(RIGHT);
	}
	if (states[SDL_SCANCODE_W]) {
		player->move(UP);
	} 
	if (states[SDL_SCANCODE_S]) {
		player->move(DOWN);
	}
	if (!states[SDL_SCANCODE_A] && !states[SDL_SCANCODE_D]) {
		player->stop(HORIZONTAL);
	}
	if (!states[SDL_SCANCODE_W] && !states[SDL_SCANCODE_S]) {
		// do nothing with gravity, without gravity player->stop(VERTICAL);
	}
	if (states[SDL_SCANCODE_R]) {
		player->resetPosition();
	}
}

void InputHandler::update() {
	SDL_Event event;
	keyStateCheck();
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
