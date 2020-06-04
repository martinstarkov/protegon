#pragma once

#include "common.h"

#include "SDL.h"

#define DEFAULT_RENDER_COLOR SDL_Color{ 255, 255, 255, 255 }

class Game {
public:
	static Game& getInstance();
	static SDL_Window* getWindow();
	static SDL_Renderer* getRenderer();
	void init();
	void loop();
	void clean();
	void quit();
public:
	int cycle;
private:
	void update();
	void render();
	void instructions();
	bool initSDL();
private:
	static std::unique_ptr<Game> _instance;
	static SDL_Window* _window;
	static SDL_Renderer* _renderer;
	static bool _running;
};

