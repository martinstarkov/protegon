#include "TextureManager.h"

#include <cassert> // assert

#include <SDL.h>
#include <SDL_image.h>

#include "debugging/Debug.h"
#include "math/Math.h"
#include "renderer/Renderer.h"

namespace ptgn {

namespace impl {

void SDLTextureManager::LoadTexture(const char* texture_key, const char* texture_path) {
	assert(texture_path != "" && "Cannot load empty texture path into sdl texture manager");
	assert(debug::FileExists(texture_path) && "Cannot load texture with non-existent file path into sdl texture manager");
	const auto key{ math::Hash(texture_key) };
	auto it{ texture_map_.find(key) };
	if (it == std::end(texture_map_)) {
		auto temp_surface{ IMG_Load( texture_path ) };
		if (temp_surface != nullptr) {
			auto& sdl_renderer{ GetSDLRenderer() };
			auto texture{ SDL_CreateTextureFromSurface(sdl_renderer.renderer_, temp_surface) };
			auto shared_texture{ std::shared_ptr<SDL_Texture>(texture, SDL_DestroyTexture) };
			texture_map_.emplace(key, shared_texture);
			SDL_FreeSurface(temp_surface);
		} else {
			debug::PrintLine("Failed to load texture into sdl texture manager: ", SDL_GetError());
		}
	} else {
		debug::PrintLine("Warning: Cannot load texture key which already exists in the sdl texture manager");
	}
}

void SDLTextureManager::UnloadTexture(const char* texture_key) {
	const auto key{ math::Hash(texture_key) }; 
	texture_map_.erase(key);
}

bool SDLTextureManager::HasTexture(const char* texture_key) {
	const auto key{ math::Hash(texture_key) };
	auto it{ texture_map_.find(key) };
	return it != std::end(texture_map_);
}

void SDLTextureManager::ResetTexture(const char* texture_key, SDL_Texture* shared_texture) {
	const auto key{ math::Hash(texture_key) };
	auto it{ texture_map_.find(key) };
	if (it != std::end(texture_map_)) {
		it->second.reset(shared_texture);
	}
}

std::shared_ptr<SDL_Texture> SDLTextureManager::GetTexture(const char* texture_key) {
	const auto key{ math::Hash(texture_key) };
	auto it{ texture_map_.find(key) };
	if (it != std::end(texture_map_)) {
		return it->second;
	}
	return nullptr;
}

SDLTextureManager& GetSDLTextureManager() {
	static SDLTextureManager default_texture_manager;
	return default_texture_manager;
}

} // namespace impl

namespace services {

interfaces::TextureManager& GetTextureManager() {
	return impl::GetSDLTextureManager();
}

} // namespace services

} // namespace ptgn