#pragma once
//#include "common.h"
//#include "defines.h"
#include "SDL.h"
//#include "TextureManager.h"
//#include "InputHandler.h"
//#include "Entity.h"
//#include "Player.h"
//#include "Camera.h"

#define DEFAULT_RENDER_COLOR SDL_Color{ 0, 0, 0, 255 }

class Game {
public:
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
	void reset();
	void clean();
	void quit();
public:
	//static std::vector<Entity*> entities;
	static bool bulletTime;
	int cycle;
	static int attempts;
private:
	void update();
	void render();
	void instructions();
	bool initSDL();
private:
	static Game* instance;
	static SDL_Window* window;
	static SDL_Renderer* renderer;
	static bool running;
	//Player* player;
	//Camera* camera;
};

