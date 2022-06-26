#include "TextureManager.h"

#include <cassert> // assert

#include <SDL.h>
#include <SDL_image.h>

#include "debugging/Debug.h"
#include "renderer/Renderer.h"
#include "core/SDLManager.h"

namespace ptgn {

namespace internal {

void SDLTextureDeleter::operator()(SDL_Texture* texture) {
	SDL_DestroyTexture(texture);
}

SDLTextureManager::SDLTextureManager() {
	GetSDLManager();
}

void SDLTextureManager::LoadTexture(const std::size_t texture_key, const char* texture_path) {
	assert(texture_path != "" && "Cannot load empty texture path into sdl texture manager");
	assert(debug::FileExists(texture_path) && "Cannot load texture with non-existent file path into sdl texture manager");
	auto temp_surface{ IMG_Load( texture_path ) };
	if (temp_surface != nullptr) {
		auto texture{ CreateTextureFromSurface(temp_surface) };
		SetTexture(texture_key, texture);
	} else {
		debug::PrintLine("Failed to load texture into sdl texture manager: ", IMG_GetError());
	}
	SDL_FreeSurface(temp_surface);
}

void SDLTextureManager::UnloadTexture(const std::size_t texture_key) {
	texture_map_.erase(texture_key);
}

SDL_Texture* SDLTextureManager::CreateTextureFromSurface(SDL_Surface* surface) const {
	auto& sdl_renderer{ GetSDLRenderer() };
	auto texture{ SDL_CreateTextureFromSurface(sdl_renderer.renderer_, surface) };
	return texture;
}

bool SDLTextureManager::HasTexture(const std::size_t texture_key) const {
	auto it{ texture_map_.find(texture_key) };
	return it != std::end(texture_map_) && it->second != nullptr;
}

void SDLTextureManager::SetTexture(const std::size_t texture_key, SDL_Texture* texture) {
	auto it{ texture_map_.find(texture_key) };
	if (it != std::end(texture_map_)) {
		if (it->second.get() != texture) {
			it->second.reset(texture);
		}
	} else {
		texture_map_.emplace(texture_key, texture);
	}
}

SDL_Texture* SDLTextureManager::GetTexture(const std::size_t texture_key) const {
	auto it{ texture_map_.find(texture_key) };
	if (it != std::end(texture_map_)) {
		return it->second.get();
	}
	return nullptr;
}

SDLTextureManager& GetSDLTextureManager() {
	static SDLTextureManager default_texture_manager;
	return default_texture_manager;
}

} // namespace internal

namespace services {

interfaces::TextureManager& GetTextureManager() {
	return internal::GetSDLTextureManager();
}

} // namespace services

} // namespace ptgn