#include "TextureManager.h"

#include <SDL_image.h>

#include "math/Math.h"
#include "renderer/ScreenRenderer.h"
#include "renderer/Surface.h"
#include "debugging/Debug.h"

namespace ptgn {

void TextureManager::Load(const char* texture_key, 
						  const char* texture_path) {
	assert(texture_path != "" && "Cannot load empty texture path");
	assert(FileExists(texture_path) && "Cannot load texture with non-existent file path");
	assert(texture_key != "" && "Cannot load invalid texture key");
	auto& instance{ GetInstance() };
	const auto key{ math::Hash(texture_key) };
	auto it{ instance.texture_map_.find(key) };
	if (it == std::end(instance.texture_map_)) {
		Surface temp_surface{ texture_path };
		Texture texture{ ScreenRenderer::CreateTexture(temp_surface) };
		temp_surface.Destroy();
		instance.texture_map_.emplace(key, texture);
	} else {
		PrintLine("Warning: Cannot load texture key which already exists in the TextureManager");
	}
}

void TextureManager::Unload(const char* texture_key) {
	auto& instance{ GetInstance() };
	const auto key{ math::Hash(texture_key) };
	auto it{ instance.texture_map_.find(key) };
	if (it != std::end(instance.texture_map_)) {
		it->second.Destroy();
		instance.texture_map_.erase(it);
	}
}

const Texture& TextureManager::GetTexture(const char* texture_key) {
	const auto& instance{ GetInstance() };
	const auto key{ math::Hash(texture_key) };
	const auto it{ instance.texture_map_.find(key) };
	assert(it != std::end(instance.texture_map_) && 
		   "Cannot retrieve texture key which does not exist in TextureManager");
	return it->second;
}

std::uint32_t& TextureManager::GetTexturePixel(void* pixels,
											   const int pitch,
											   const V2_int& position, 
											   PixelFormat format) {
	// Bytes per pixel.
	const int bytes_per_pixel{ 
		SDL_BYTESPERPIXEL(static_cast<SDL_PixelFormatEnum>(format)) 
	};
	assert(bytes_per_pixel == 4 && 
		   "Invalid bytes per pixel for texture pixel access");
	std::uint32_t* pixel{ 
		(std::uint32_t*)pixels + position.y * pitch + position.x * bytes_per_pixel 
	};
	return *pixel;
}

void TextureManager::Destroy() {
	auto& instance{ GetInstance() };
	for (auto& pair : instance.texture_map_) {
		pair.second.Destroy();
	}
	instance.texture_map_.clear();
}

} // namespace ptgn