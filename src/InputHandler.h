#pragma once

#include "common.h"

#include "SDL.h"

class InputHandler {
public:
	static InputHandler& getInstance();
	static void update();
private:
	static std::unique_ptr<InputHandler> _instance;
};

