#pragma once
#include "SDL.h"
#include "common.h"

class InputHandler {
public:
	InputHandler();
	static InputHandler* getInstance() {
		if (!instance) {
			instance = new InputHandler();
		}
		return instance;
	}
	static void update();
	static const Uint8* getKeyStates() {
		return SDL_GetKeyboardState(NULL);
	}
private:
	static void keyStateCheck();
	static void keyPress(SDL_KeyboardEvent press);
	static void keyRelease(SDL_KeyboardEvent release);
	static void playerMotion();
	static void cameraMotion();
private:
	static InputHandler* instance;
	static const Uint8* states;
	static const Uint8* previousStates;
};

