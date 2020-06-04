#pragma once

#include <map>

#include "common.h"

#include "SDL_image.h"

class TextureManager {
public:
	static TextureManager& getInstance();
	static SDL_Texture* load(const char* path);
	static void draw(SDL_Texture* texture, SDL_Rect source, SDL_Rect destination);
	static void draw(SDL_Texture* texture, SDL_Rect source, SDL_Rect destination, float angle, SDL_RendererFlip flip);
	static void draw(SDL_Rect rectangle, SDL_Color color = { 0, 0, 0, 255 });
	//static void draw(std::string id, AABB box, float angle = 0.0f, SDL_RendererFlip flip = SDL_FLIP_NONE);
	static void removeTexture(const char* path);
private:
	static std::unique_ptr<TextureManager> _instance;
	static std::map<const char*, SDL_Texture*> textureMap;
};

