#pragma once
#include "SDL.h"
#include "common.h"

class InputHandler {
private:
	static InputHandler* instance;
	void keyStateCheck();
	void keyPress(SDL_KeyboardEvent press);
	void keyRelease(SDL_KeyboardEvent release);
public:
	InputHandler();
	static InputHandler* getInstance() {
		if (!instance) {
			instance = new InputHandler();
		}
		return instance;
	}
	void update();
};

