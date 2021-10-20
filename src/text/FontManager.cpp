#include "FontManager.h"

#include <cassert> // assert

#include <SDL_ttf.h>

#include "debugging/Debug.h"
#include "math/Math.h"

namespace ptgn {

namespace impl {

void SDLFontManager::LoadFont(const std::size_t font_key, const char* font_path, std::uint32_t point_size, std::uint32_t index) {
	assert(font_path != "" && "Cannot load empty font path into sdl font manager");
	assert(debug::FileExists(font_path) && "Cannot load font with non-existent file path into sdl font manager");
	auto it{ font_map_.find(font_key) };
	if (it == std::end(font_map_)) {
		auto font{ TTF_OpenFontIndex(font_path, point_size, index) };
		if (font != nullptr) {
			auto shared_font{ std::shared_ptr<TTF_Font>(font, TTF_CloseFont) };
			font_map_.emplace(font_key, shared_font);
		} else {
			debug::PrintLine("Failed to load font into sdl font manager: ", TTF_GetError());
		}
	} else {
		debug::PrintLine("Warning: Cannot load font key which already exists in the sdl font manager");
	}
}

void SDLFontManager::UnloadFont(const std::size_t font_key) {
	font_map_.erase(font_key);
}

bool SDLFontManager::HasFont(const std::size_t font_key) const {
	auto it{ font_map_.find(font_key) };
	return it != std::end(font_map_) && it->second != nullptr;
}

std::int32_t SDLFontManager::GetHeight(const std::size_t font_key) const {
	auto it{ font_map_.find(font_key) };
	if (it != std::end(font_map_)) {
		assert(it->second != nullptr && "Cannot get font height of non-existent font");
		return TTF_FontHeight(it->second.get());
	}
	return 0;
}

std::shared_ptr<TTF_Font> SDLFontManager::GetFont(const std::size_t font_key) {
	auto it{ font_map_.find(font_key) };
	if (it != std::end(font_map_)) {
		return it->second;
	}
	return nullptr;
}

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