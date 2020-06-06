
#include "TextureManager.h"
#include "Game.h"

std::unique_ptr<TextureManager> TextureManager::_instance = nullptr;
std::map<std::string, SDL_Texture*> TextureManager::_textureMap;

TextureManager& TextureManager::getInstance() {
	if (!_instance) {
		_instance = std::make_unique<TextureManager>();
	}
	return *_instance;
}

SDL_Texture* TextureManager::load(std::string path) {
	if (path != "") {
		auto iterator = _textureMap.find(path);
		if (iterator != _textureMap.end()) { // return if texture exists
			return iterator->second;
		}
		SDL_Texture* texture = nullptr;
		SDL_Surface* tempSurface = IMG_Load(path.c_str());
		if (tempSurface == 0) {
			std::cout << "Failed to IMG_Load path: '" << path << "'" << std::endl;
			return texture;
		}
		texture = SDL_CreateTextureFromSurface(Game::getRenderer(), tempSurface);
		SDL_FreeSurface(tempSurface);
		if (texture) {
			_textureMap.emplace(path, texture);
		} else {
			std::cout << "Failed to SDL_CreateTextureFromSurface with path: '" << path << "'" << std::endl;
		}
		return texture;
	}
	return nullptr;
}

SDL_Texture* TextureManager::getTexture(std::string path) {
	auto iterator = _textureMap.find(path);
	if (iterator != _textureMap.end()) { // exit early if texture exists
		return iterator->second;
	}
	return nullptr;
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

void TextureManager::removeTexture(std::string path) {
	if (path != "") {
		_textureMap.erase(path);
	}
}
