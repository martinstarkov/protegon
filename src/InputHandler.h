#pragma once
#include "SDL.h"
#include "common.h"

class InputHandler {
private:
	static InputHandler* instance;
	void keyStateChange();
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

