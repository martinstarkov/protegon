#pragma once
#include "common.h"
#include "defines.h"
#include "SDL.h"
#include "TextureManager.h"
#include "InputHandler.h"
#include "Entity.h"
#include "Player.h"
#include "Camera.h"

class Game {
private:
	static Game* instance;
	static SDL_Window* window;
	static SDL_Renderer* renderer;
	static bool running;
	TextureManager* tm;
	InputHandler* ih;
	Player* player;
	Camera* camera;
	void update();
	void render();
	void instructions();
public:
	static bool bulletTime;
	static std::vector<Entity*> entities;
	static std::vector<Entity*> entityObjects;
	static std::vector<AABB> broadphase;
	static Uint32 time;
	static Uint32 previousTime;
	int cycle;
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
	static void reset();
	void clean();
	void quit();
};

