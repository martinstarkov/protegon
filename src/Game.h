#pragma once

#include "common.h"

#include "AABB.h"

#include "SDL.h"

class Game {
public:
	static std::vector<AABB> aabbs;
	static std::vector<std::pair<Vec2D, Vec2D>> lines;
	static std::vector<Vec2D> points;
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

