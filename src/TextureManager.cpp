#include "TextureManager.h"
#include "Game.h"

TextureManager* TextureManager::instance = nullptr;

TextureManager::TextureManager() {
	SDL_Surface* surface = IMG_Load(TEST_PATH);
	texture = SDL_CreateTextureFromSurface(Game::getRenderer(), surface);
	SDL_FreeSurface(surface);
}

void TextureManager::draw(std::string name, SDL_Rect* srcRect, SDL_Rect* destRect) {
	if (name == "tile") {
		SDL_RenderCopyEx(Game::getRenderer(), texture, srcRect, destRect, NULL, NULL, SDL_FLIP_NONE);
	}
}
