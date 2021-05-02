#include "TextureManager.h"

#include <cassert> // assert

#include <SDL_image.h>

#include "math/Hasher.h"

#include "core/Engine.h"

#include "debugging/Debug.h"

namespace engine {

std::unordered_map<std::size_t, Texture> TextureManager::texture_map_;

void TextureManager::Load(const char* texture_key, const char* texture_path, std::size_t display_index) {
	assert(texture_path != "" && "Cannot load empty texture path");
	assert(FileExists(texture_path) && "Cannot load texture with non-existent file path");
	assert(texture_key != "" && "Cannot load invalid texture key");
	auto key{ Hasher::HashCString(texture_key) };
	auto it{ texture_map_.find(key) };
	if (it == std::end(texture_map_)) { 
		Surface temp_surface{ texture_path };
		Texture texture{ Engine::GetDisplay(display_index).second, temp_surface };
		temp_surface.Destroy();
		texture_map_.emplace(key, texture);
	} else {
		PrintLine("Warning: Cannot load texture key which already exists in the TextureManager");
	}
}

Texture TextureManager::GetTexture(const char* texture_key) {
	auto key{ Hasher::HashCString(texture_key) };
	auto it{ texture_map_.find(key) };
	assert(it != std::end(texture_map_) && "Cannot retrieve texture key which does not exist in TextureManager");
	return it->second;
}

std::uint32_t& TextureManager::GetTexturePixel(void* pixels, const int pitch, const V2_int& position) {
	// Source: http://sdl.beuc.net/sdl.wiki/Pixel_Access
	//int bpp = surface->format->BytesPerPixel;
	int bpp{ sizeof(std::uint32_t) };
	/* Here p is the address to the pixel we want to retrieve */
	auto p{ (std::uint8_t*)pixels + position.y * pitch + position.x * bpp };
	return *(std::uint32_t*)p;
}

void TextureManager::Clear() {
	for (auto& pair : texture_map_) {
		pair.second.Destroy();
	}
	texture_map_.clear();
}

void TextureManager::Remove(const char* texture_key) {
	auto key{ Hasher::HashCString(texture_key) };
	auto it{ texture_map_.find(key) };
	if (it != std::end(texture_map_)) {
		it->second.Destroy();
		texture_map_.erase(it);
	}
}

} // namespace engine