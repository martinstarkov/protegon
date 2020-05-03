#pragma once
#include "SDL.h"
#include "SDL_image.h"
#include "defines.h"
#include "common.h"

#define TEST_PATH "../resources/test_tile_1.png"

class TextureManager {
private:
	static TextureManager* instance;
	static SDL_Window* window;
	static SDL_Renderer* renderer;
	static SDL_Texture* texture;
public:
	TextureManager();
	void draw(std::string name, SDL_Rect* srcRect, SDL_Rect* destRect);
	static TextureManager* getInstance() {
		if (!instance) {
			instance = new TextureManager();
		}
		return instance;
	}
	static SDL_Window* getWindow() {
		return window;
	}
	static SDL_Renderer* getRenderer() {
		return renderer;
	}
	void clean();
};

