#pragma once
#include "SDL_image.h"
#include <map>

class TextureManager {
private:
	static TextureManager* instance;
	static std::map<const char*, SDL_Texture*> textureMap;
public:
	static TextureManager* getInstance() {
		if (!instance) {
			instance = new TextureManager();
		}
		return instance;
	}
	static SDL_Texture* load(const char* path);
	static void draw(SDL_Texture* texture, SDL_Rect source, SDL_Rect destination);
	static void draw(SDL_Texture* texture, SDL_Rect source, SDL_Rect destination, float angle, SDL_RendererFlip flip);
	static void draw(SDL_Rect rectangle, SDL_Color color = { 0, 0, 0, 255 });
	//static void draw(std::string id, AABB box, float angle = 0.0f, SDL_RendererFlip flip = SDL_FLIP_NONE);
	static void removeTexture(const char* path);
};

