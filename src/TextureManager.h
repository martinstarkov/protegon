#pragma once

#include <map>
#include <string>

#include "common.h"

#include "SDL_image.h"

class TextureManager {
public:
	static TextureManager& getInstance();
	static SDL_Texture* load(std::string path);
	static SDL_Texture* getTexture(const std::string& path);
	static void draw(SDL_Texture* texture, SDL_Rect source, SDL_Rect destination);
	static void draw(SDL_Texture* texture, SDL_Rect source, SDL_Rect destination, double angle, SDL_RendererFlip flip);
	static void draw(SDL_Rect rectangle, SDL_Color color = { 0, 0, 0, 255 });
	static void removeTexture(std::string path);
private:
	static std::unique_ptr<TextureManager> _instance;
	static std::map<std::string, SDL_Texture*> _textureMap;
};

