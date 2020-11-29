#include "FontManager.h"

#include <SDL.h>
#include <SDL_ttf.h>
#include <cassert> // assert

#include "core/Engine.h"
#include "utils/Hasher.h"

namespace engine {

std::unordered_map<std::size_t, Texture> FontManager::font_map_;

void FontManager::Load(const char* text, const Color& color, const int size, const char* font_path) {
	assert(font_path != "" && "Cannot load empty font path");
	assert(text != "" && "Cannot load invalid font text");
	auto key = Hasher::HashCString(text);
	auto it = font_map_.find(key);
	if (it == std::end(font_map_)) { // Only create font if it doesn't already exist in map.
		TTF_Font* font = TTF_OpenFont(font_path, size);
		assert(font != nullptr && "Failed to load true type font");
		SDL_Surface* temp_surface = TTF_RenderText_Solid(font, text, color);
		assert(temp_surface != nullptr && "Failed to load font text into surface");
		SDL_Texture* texture = SDL_CreateTextureFromSurface(Engine::GetRenderer(), temp_surface);
		SDL_FreeSurface(temp_surface);
		assert(texture != nullptr && "Failed to create font texture from surface");
		font_map_.emplace(key, texture);
		TTF_CloseFont(font);
	}
}

Texture FontManager::GetFont(const char* font_key) {
	auto key = Hasher::HashCString(font_key);
	auto it = font_map_.find(key);
	assert(it != std::end(font_map_) && "Font does not exist in font manager");
	return it->second;
}

void FontManager::Draw(const char* text, V2_int position, V2_int size) {
	SDL_Rect rect{ position.x, position.y, size.x, size.y };
	SDL_RenderCopy(Engine::GetRenderer(), GetFont(text), NULL, &rect);
}

void FontManager::RemoveFont(const char* font_key) {
	auto key = Hasher::HashCString(font_key);
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