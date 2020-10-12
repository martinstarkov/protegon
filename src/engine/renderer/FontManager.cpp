#include "FontManager.h"

#include <SDL_ttf.h>

#include <cassert>

#include <engine/core/Engine.h>

namespace engine {

std::unordered_map<std::string, SDL_Texture*> FontManager::font_map_;

SDL_Texture& FontManager::Load(std::string& text, const Color& color, const int size, const char* path) {
	assert(path != "" && "Cannot load empty font path");
	assert(text != "" && "Cannot load invalid font text");
	auto it = font_map_.find(text);
	if (it != std::end(font_map_)) { // Don't create font if it already exists in map.
		return *it->second;
	}
	TTF_Font* font = TTF_OpenFont(path, size);
	assert(font != nullptr && "Failed to load true type font");
	SDL_Surface* temp_surface = TTF_RenderText_Solid(font, text.c_str(), color);
	assert(temp_surface != nullptr && "Failed to load font text into surface");
	 SDL_Texture* texture = SDL_CreateTextureFromSurface(&Engine::GetRenderer(), temp_surface);
	SDL_FreeSurface(temp_surface);
	assert(texture != nullptr && "Failed to create font texture from surface");
	font_map_.emplace(text, texture);
	TTF_CloseFont(font);
	return *texture;
}

SDL_Texture& FontManager::GetFont(const std::string& key) {
	auto it = font_map_.find(key);
	assert(it != std::end(font_map_) && "Font does not exist in font manager");
	return *it->second;
}

void FontManager::Draw(const std::string& text, V2_int position, V2_int size) {
	SDL_Rect rect{ position.x, position.y, size.x, size.y };
	SDL_RenderCopy(&Engine::GetRenderer(), &GetFont(text), NULL, &rect);
}

void FontManager::RemoveFont(const std::string& key) {
	auto it = font_map_.find(key);
	if (it != std::end(font_map_)) {
		SDL_DestroyTexture(it->second);
		font_map_.erase(it);
	}
}

void FontManager::Clean() {
	for (auto& pair : font_map_) {
		SDL_DestroyTexture(pair.second);
	}
	font_map_.clear();
}

} // namespace engine