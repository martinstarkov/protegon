#pragma once
#include "common.h"
#include "SDL_image.h"

#define TEST_PATH "../resources/test_tile_1.png"

class TextureManager {
private:
	static TextureManager* instance;
	SDL_Texture* texture;
public:
	TextureManager();
	void draw(std::string name, SDL_Rect* srcRect, SDL_Rect* destRect);
	static TextureManager* getInstance() {
		if (!instance) {
			instance = new TextureManager();
		}
		return instance;
	}
};

