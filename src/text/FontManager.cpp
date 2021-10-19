#include "FontManager.h"

#include <cassert> // assert

#include <SDL.h>
#include <SDL_ttf.h>

#include "debugging/Debug.h"
#include "math/Math.h"
#include "renderer/Renderer.h"

namespace ptgn {

namespace impl {

SDLFontManager::~SDLFontManager() {
	for (auto& [key, font] : font_map_) {
		//SDL_DestroyFont(font.get());
	}
}

void SDLFontManager::LoadFont(const char* font_key, const char* font_path) {
	assert(font_path != "" && "Cannot load empty font path into sdl font manager");
	assert(debug::FileExists(font_path) && "Cannot load font with non-existent file path into sdl font manager");
	const auto key{ math::Hash(font_key) };
	auto it{ font_map_.find(key) };
	if (it == std::end(font_map_)) {
		// auto temp_surface{ IMG_Load( font_path ) };
		// if (temp_surface != nullptr) {
		// 	auto& sdl_renderer{ GetSDLRenderer() };
		// 	auto font{ SDL_CreateFontFromSurface(sdl_renderer.renderer_, temp_surface) };
		// 	auto shared_font{ std::shared_ptr<SDL_Font>(font, SDL_DestroyFont) };
		// 	font_map_.emplace(key, shared_font);
		// 	SDL_FreeSurface(temp_surface);
		// } else {
		// 	debug::PrintLine("Failed to load font into sdl font manager: ", SDL_GetError());
		// }
	} else {
		debug::PrintLine("Warning: Cannot load font key which already exists in the sdl font manager");
	}
}

void SDLFontManager::UnloadFont(const char* font_key) {
	const auto key{ math::Hash(font_key) }; 
	font_map_.erase(key);
}

// std::shared_ptr<SDL_Font> SDLFontManager::GetFont(const char* font_key) {
// 	const auto key{ math::Hash(font_key) };
// 	auto it{ font_map_.find(key) };
// 	if (it != std::end(font_map_)) {
// 		return it->second;
// 	}
// 	return nullptr;
// }

SDLFontManager& GetSDLFontManager() {
	static SDLFontManager default_font_manager;
	return default_font_manager;
}

} // namespace impl

namespace services {

interfaces::FontManager& GetFontManager() {
	return impl::GetSDLFontManager();
}

} // namespace services

} // namespace ptgn