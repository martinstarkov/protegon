#include "TextureManager.h"

TextureManager* TextureManager::instance = nullptr;
SDL_Window* TextureManager::window = nullptr;
SDL_Renderer* TextureManager::renderer = nullptr;
SDL_Texture* TextureManager::texture = nullptr;

TextureManager::TextureManager() {
	window = SDL_CreateWindow(WINDOW_TITLE, WINDOW_X, WINDOW_Y, WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_FLAGS);
	if (window) {
		renderer = SDL_CreateRenderer(window, -1, 0);
		if (renderer) {
			std::cout << "SDL window and renderer init successful" << std::endl;
			SDL_Surface* surface = IMG_Load(TEST_PATH);
			texture = SDL_CreateTextureFromSurface(renderer, surface);
			SDL_FreeSurface(surface);
		} else {
			std::cout << "SDL renderer failed to init" << std::endl;
		}
	} else {
		std::cout << "SDL window failed to init" << std::endl;
	}
}

void TextureManager::draw(std::string name, SDL_Rect* srcRect, SDL_Rect* destRect) {
	if (name == "tile") {
		SDL_RenderCopyEx(renderer, texture, srcRect, destRect, NULL, NULL, SDL_FLIP_NONE);
	}
}

void TextureManager::clean() {
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
}
