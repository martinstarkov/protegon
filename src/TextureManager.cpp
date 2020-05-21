#include "TextureManager.h"
#include "Game.h"

TextureManager* TextureManager::instance = nullptr;
std::map<std::string, SDL_Texture*> TextureManager::textureMap;

void TextureManager::load(std::string id, std::string path) {
	SDL_Surface* tempSurface = IMG_Load(path.c_str());
	if (tempSurface == 0) {
		std::cout << "Failed to IMG_Load path: '" << path << "'" << std::endl;
		return;
	}
	SDL_Texture* texture = SDL_CreateTextureFromSurface(Game::getRenderer(), tempSurface);
	SDL_FreeSurface(tempSurface);
	if (texture) {
		if (textureMap.find(id) == textureMap.end()) { // add texture to map
			textureMap.insert({ id, texture }); 
		} else { // already contains texture, replace
			textureMap[id] = texture;
		}
	} else {
		std::cout << "Failed to SDL_CreateTextureFromSurface with path: '" << path << "'" << std::endl;
	}
}

void TextureManager::draw(std::string id, SDL_Rect* source, SDL_Rect* destination, float angle, SDL_RendererFlip flip) {
	if (textureMap.find(id) != textureMap.end()) {
		SDL_RenderCopyEx(Game::getRenderer(), textureMap[id], source, destination, (double)angle, NULL, flip);
	}
}

void TextureManager::removeTexture(std::string id) {
	textureMap.erase(id);
}
