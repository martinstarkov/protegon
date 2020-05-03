#pragma once
#include "SDL.h"
#include "common.h"
#include "TextureManager.h"

class Game {
private:
	static Game* instance;
	static TextureManager* tm;
	static bool running;
	static std::string input;
	void update();
	void render();
public:
	static Game* getInstance() {
		if (!instance) {
			instance = new Game();
		}
		return instance;
	}
	void init();
	void loop();
	void quit();
};

