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
		player->accelerate(Keys::LEFT);
	}
	if (states[SDL_SCANCODE_D]) {
		player->accelerate(Keys::RIGHT);
	}
	if (states[SDL_SCANCODE_W]) {
		player->accelerate(Keys::UP);
	} 
	if (states[SDL_SCANCODE_S]) {
		player->accelerate(Keys::DOWN);
	}
	if (!states[SDL_SCANCODE_A] && !states[SDL_SCANCODE_D]) {
		player->stop(Axis::HORIZONTAL);
	}
	if (!states[SDL_SCANCODE_W] && !states[SDL_SCANCODE_S]) {
		// do nothing with gravity, without gravity player->stop(VERTICAL);
	}
}

void InputHandler::keyPress(SDL_KeyboardEvent press) {
	if (press.keysym.scancode == SDL_SCANCODE_R) {
		Game::reset();
	}
}

void InputHandler::keyRelease(SDL_KeyboardEvent release) {}

void InputHandler::update() {
	SDL_Event event;
	keyStateCheck();
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
				Game::getInstance()->quit();
				break;
			case SDL_KEYDOWN:
				keyPress(event.key);
				break;
			case SDL_KEYUP:
				keyRelease(event.key);
				break;
			default:
				break;
		}
	}
}
