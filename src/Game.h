#pragma once

#include "common.h"

#include "AABB.h"

#include "SDL.h"

#include <tuple>

class Game {
public:
	static std::vector<std::pair<AABB, SDL_Color>> aabbs;
	static std::vector<std::tuple<Vec2D, Vec2D, SDL_Color>> lines;
	static std::vector<std::pair<Vec2D, SDL_Color>> points;
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

