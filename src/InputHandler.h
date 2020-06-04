#pragma once

#include "common.h"

#include "SDL.h"

class InputHandler {
public:
	static InputHandler& getInstance();
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
	static std::unique_ptr<InputHandler> _instance;
	static const Uint8* _states;
};

