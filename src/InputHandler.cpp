
#include "InputHandler.h"
#include "Game.h"

std::unique_ptr<InputHandler> InputHandler::_instance = nullptr;

InputHandler& InputHandler::getInstance() {
	if (!_instance) {
		_instance = std::make_unique<InputHandler>();
	}
	return *_instance;
}


void InputHandler::update(SDL_Event& event) {
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
				Game::getInstance().quit();
				break;
			default:
				break;
		}
	}
}
