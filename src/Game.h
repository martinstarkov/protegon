#pragma once
#include "common.h"
#include "defines.h"
#include "SDL.h"
#include "TextureManager.h"
#include "InputHandler.h"
#include "Player.h"

class Game {
private:
	static Game* instance;
	static SDL_Window* window;
	static SDL_Renderer* renderer;
	static bool running;
	TextureManager* tm;
	InputHandler* ih;
	Player* player;
	void update();
	void render();
	void instructions();
public:
	static std::vector<Entity*> entities;
	static std::vector<AABB> broadphase;
	Game();
	static Game* getInstance() {
		if (!instance) {
			instance = new Game();
		}
		return instance;
	}
	static SDL_Window* getWindow() {
		return window;
	}
	static SDL_Renderer* getRenderer() {
		return renderer;
	}
	void init();
	void loop();
	void clean();
	void quit();
};

