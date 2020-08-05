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

void TextureManager::drawPoint(Vec2D point, SDL_Color color) {
	setDrawColor(color);
	SDL_RenderDrawPoint(Game::getRenderer(), static_cast<int>(round(point.x)), static_cast<int>(round(point.y)));
	setDrawColor(RENDER_COLOR);
}

void TextureManager::drawLine(Vec2D origin, Vec2D destination, SDL_Color color) {
	setDrawColor(color);
	SDL_RenderDrawLine(Game::getRenderer(), static_cast<int>(round(origin.x)), static_cast<int>(round(origin.y)), static_cast<int>(round(destination.x)), static_cast<int>(round(destination.y)));
	setDrawColor(RENDER_COLOR);
}

void TextureManager::drawLine(Ray2D ray, SDL_Color color) {
	drawLine(ray.origin, ray.origin + ray.direction, color);
}

void TextureManager::drawRectangle(SDL_Rect rectangle, SDL_Color color) {
	setDrawColor(color);
	SDL_RenderDrawRect(Game::getRenderer(), &rectangle);
	setDrawColor(RENDER_COLOR);
}

void TextureManager::drawRectangle(Vec2D position, Vec2D size, SDL_Color color) {
	drawRectangle(Util::RectFromVec(position, size), color);
}

void TextureManager::drawRectangle(AABB rectangle, SDL_Color color) {
	drawRectangle(Util::RectFromAABB(rectangle), color);
}

void TextureManager::drawRectangle(SDL_Texture* texture, SDL_Rect source, SDL_Rect destination, double angle, SDL_RendererFlip flip) {
	assert(texture && "Attempting to draw null texture");
	SDL_RenderCopyEx(Game::getRenderer(), texture, &source, &destination, angle, NULL, flip);
}

void TextureManager::removeTexture(std::string path) {
	assert(path != "" && "Attempting to remove non-existent texture");
	_textureMap.erase(path);
}
