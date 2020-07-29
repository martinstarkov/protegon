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
	assert(path != "" && "Cannot load empty path");
	auto iterator = _textureMap.find(path);
	if (iterator != _textureMap.end()) { // don't create if texture already exists in map
		return iterator->second;
	}
	SDL_Texture* texture = nullptr;
	SDL_Surface* tempSurface = IMG_Load(path.c_str());
	assert(tempSurface && "Failed to load image into surface");
	texture = SDL_CreateTextureFromSurface(Game::getRenderer(), tempSurface);
	SDL_FreeSurface(tempSurface);
	assert(texture && "Failed to create texture from surface");
	_textureMap.emplace(path, texture);
	return texture;
}

SDL_Texture* TextureManager::getTexture(const std::string& path) {
	auto iterator = _textureMap.find(path);
	assert(iterator != _textureMap.end() && "Attempting to retrieve unloaded texture");
	return iterator->second;
}

void TextureManager::setDrawColor(SDL_Color color) {
	SDL_SetRenderDrawColor(Game::getRenderer(), color.r, color.g, color.b, color.a);
}

void TextureManager::draw(SDL_Rect rectangle, SDL_Color color) {
	setDrawColor(color);
	SDL_RenderDrawRect(Game::getRenderer(), &rectangle);
	setDrawColor(DEFAULT_RENDER_COLOR);
}

void TextureManager::draw(SDL_Texture* texture, SDL_Rect source, SDL_Rect destination, double angle, SDL_RendererFlip flip) {
	assert(texture && "Attempting to draw null texture");
	SDL_RenderCopyEx(Game::getRenderer(), texture, &source, &destination, angle, NULL, flip);
}

void TextureManager::removeTexture(std::string path) {
	assert(path != "" && "Attempting to remove non-existent texture");
	_textureMap.erase(path);
}
