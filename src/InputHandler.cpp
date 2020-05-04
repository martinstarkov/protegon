#include "InputHandler.h"
#include "Game.h"

InputHandler* InputHandler::instance = nullptr;

InputHandler::InputHandler() {}

void InputHandler::keyStateChange() {
	const Uint8* states = SDL_GetKeyboardState(NULL);
	int velocity = 3;
	if (states[SDL_SCANCODE_A]) {
		Game::x -= velocity;
		std::cout << "Pressed A" << std::endl;
	}
	if (states[SDL_SCANCODE_D]) {
		Game::x += velocity;
		std::cout << "Pressed D" << std::endl;
	}
	if (states[SDL_SCANCODE_W]) {
		Game::y -= velocity;
		std::cout << "Pressed W" << std::endl;
	} 
	if (states[SDL_SCANCODE_S]) {
		Game::y += velocity;
		std::cout << "Pressed S" << std::endl;
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
