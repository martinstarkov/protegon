#include "TextureManager.h"
#include "Game.h"
#include <iostream>

TextureManager* TextureManager::instance = nullptr;
std::map<const char*, SDL_Texture*> TextureManager::textureMap;

SDL_Texture* TextureManager::load(const char* path) {
	SDL_Texture* texture = nullptr;
	SDL_Surface* tempSurface = IMG_Load(path);
	if (tempSurface == 0) {
		std::cout << "Failed to IMG_Load path: '" << path << "'" << std::endl;
		return texture;
	}
	texture = SDL_CreateTextureFromSurface(Game::getRenderer(), tempSurface);
	SDL_FreeSurface(tempSurface);
	if (texture) {
		if (textureMap.find(path) == textureMap.end()) { // add texture to map
			textureMap.insert({ path, texture }); 
		} else { // already contains texture, replace
			textureMap[path] = texture;
		}
	} else {
		std::cout << "Failed to SDL_CreateTextureFromSurface with path: '" << path << "'" << std::endl;
	}
	return texture;
}

void TextureManager::draw(SDL_Texture* texture, SDL_Rect source, SDL_Rect destination) {
	if (texture) {
		SDL_RenderCopy(Game::getRenderer(), texture, &source, &destination);
	}
}

void TextureManager::draw(SDL_Rect rectangle, SDL_Color color) {
	SDL_SetRenderDrawColor(Game::getRenderer(), color.r, color.g, color.b, color.a);
	SDL_RenderDrawRect(Game::getRenderer(), &rectangle);
	SDL_SetRenderDrawColor(Game::getRenderer(), DEFAULT_RENDER_COLOR.r, DEFAULT_RENDER_COLOR.g, DEFAULT_RENDER_COLOR.b, DEFAULT_RENDER_COLOR.a);
}

void TextureManager::draw(SDL_Texture* texture, SDL_Rect source, SDL_Rect destination, float angle, SDL_RendererFlip flip) {
	if (texture) {
		SDL_RenderCopyEx(Game::getRenderer(), texture, &source, &destination, (double)angle, NULL, flip);
	}
}

void TextureManager::removeTexture(const char* path) {
	textureMap.erase(path);
}
