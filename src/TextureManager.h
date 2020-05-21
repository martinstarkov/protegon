#pragma once
#include "common.h"
#include "SDL_image.h"
#include <string>
#include <map>

class TextureManager {
private:
	static TextureManager* instance;
	static std::map<std::string, SDL_Texture*> textureMap;
public:
	static TextureManager* getInstance() {
		if (!instance) {
			instance = new TextureManager();
		}
		return instance;
	}
	static void load(std::string id, std::string path);
	static void draw(std::string id, SDL_Rect* source, SDL_Rect* destination, float angle = 0.0f, SDL_RendererFlip flip = SDL_FLIP_NONE);
	static void removeTexture(std::string id);
};

