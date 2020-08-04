#pragma once

#include <map>
#include <string>

#include "common.h"

#include "Vec2D.h"
#include "Ray2D.h"
#include "AABB.h"

#include "SDL_image.h"

// TODO: Add draw methods with const references in addition to current copy versions

class TextureManager {
public:
	static TextureManager& getInstance();
	static SDL_Texture* load(std::string path);
	static SDL_Texture* getTexture(const std::string& path);
	static void setDrawColor(SDL_Color color);
	static void drawPoint(Vec2D point, SDL_Color color = { 0, 0, 0, 255 });
	static void drawLine(Vec2D origin, Vec2D destination, SDL_Color color = { 0, 0, 0, 255 });
	static void drawLine(Ray2D ray, SDL_Color color = { 0, 0, 0, 255 });
	static void drawRectangle(SDL_Rect rectangle, SDL_Color color = { 0, 0, 0, 255 });
	static void drawRectangle(Vec2D position, Vec2D size, SDL_Color color = { 0, 0, 0, 255 });
	static void drawRectangle(AABB rectangle, SDL_Color color = { 0, 0, 0, 255 });
	static void drawRectangle(SDL_Texture* texture, SDL_Rect source, SDL_Rect destination, double angle = 0.0, SDL_RendererFlip flip = SDL_FLIP_NONE);
	static void removeTexture(std::string path);
private:
	static std::unique_ptr<TextureManager> _instance;
	static std::map<std::string, SDL_Texture*> _textureMap;
};

