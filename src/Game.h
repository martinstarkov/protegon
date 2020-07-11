#pragma once

#include "common.h"

#include "SDL.h"

class Game {
public:
	static Game& getInstance();
	static SDL_Window* getWindow();
	static SDL_Renderer* getRenderer();
	void init();
	void loop();
	void clean();
	void quit();
private:
	void update();
	void render();
	void instructions();
	void initSDL(const char* title, int x, int y, int w, int h, Uint32 flags);
	static std::unique_ptr<Game> _instance;
	static SDL_Window* _window;
	static SDL_Renderer* _renderer;
	static bool _running;
};

