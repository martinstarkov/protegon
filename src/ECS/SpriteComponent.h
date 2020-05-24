#pragma once
#include "Components.h"
#include "SDL.h"
#include "../TextureManager.h"

class SpriteComponent : public Component {
public:
	SpriteComponent() {
		_texture = nullptr;
		_source = { 0, 0, 0, 0 };
	}
	SpriteComponent(const char* path, AABB spriteRectangle) : SpriteComponent() {
		_source = spriteRectangle.AABBtoRect();
		_texture = TextureManager::load(path);
	}
	SDL_Texture* getTexture() {
		return _texture;
	}
	void setTexture(SDL_Texture* texture) {
		_texture = texture;
	}
	SDL_Rect getSource() {
		return _source;
	}
	void setSource(SDL_Rect source) {
		_source = source;
	}
private:
	SDL_Texture* _texture;
	SDL_Rect _source;
};