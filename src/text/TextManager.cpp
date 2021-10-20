#include "TextManager.h"

#include <cassert> // assert

#include <SDL_ttf.h>

#include "debugging/Debug.h"
#include "math/Math.h"
#include "renderer/Renderer.h"
#include "texture/TextureManager.h"
#include "text/FontManager.h"

namespace ptgn {

namespace impl {

SDLTextManager::~SDLTextManager() {
	auto& sdl_texture_manager{ GetSDLTextureManager() };
	for (const auto& [key, sdl_text] : text_map_) {
		sdl_texture_manager.UnloadTexture(key);
	}
}

void SDLTextManager::LoadText(const std::size_t text_key, const char* content, const std::size_t font_key, const Color& color) {
	auto it{ text_map_.find(text_key) };
	if (it == std::end(text_map_)) {
		text_map_.emplace(text_key, content, font_key, color);
	} else {
		debug::PrintLine("Warning: Cannot load text key which already exists in the default text manager");
	}
}

void SDLTextManager::UnloadText(const std::size_t text_key) {
	auto it{ text_map_.find(text_key) };
	auto& sdl_texture_manager{ GetSDLTextureManager() };
	if (it != std::end(text_map_)) {
		sdl_texture_manager.UnloadTexture(text_key);
		text_map_.erase(it);
	}
}

void SDLTextManager::SetTextContent(const std::size_t text_key, const char* new_content) {
	auto it{ text_map_.find(text_key) };
	if (it != std::end(text_map_)) {
		auto& text{ it->second };
		text.content_ = new_content;
		RefreshText(text_key, text);
	}
}

void SDLTextManager::SetTextColor(const std::size_t text_key, const Color& new_color) {
	auto it{ text_map_.find(text_key) };
	if (it != std::end(text_map_)) {
		auto& text{ it->second };
		text.color_ = new_color;
		RefreshText(text_key, text);
	}
}

void SDLTextManager::SetTextFont(const std::size_t text_key, const std::size_t new_font_key) {
	auto& sdl_font_manager{ GetSDLFontManager() };
	assert(sdl_font_manager.HasFont(new_font_key) && "Cannot set sdl text font which has not been loaded into the sdl font manager");
	auto it{ text_map_.find(text_key) };
	if (it != std::end(text_map_)) {
		auto& text{ it->second };
		text.font_key_ = new_font_key;
		RefreshText(text_key, text);
	}
}

void SDLTextManager::RefreshText(const std::size_t text_key, const SDLText& text) {
	auto& sdl_font_manager{ GetSDLFontManager() };
	auto font{ sdl_font_manager.GetFont(text.font_key_) };
	TTF_SetFontStyle(font.get(), text.style_);
	SDL_Surface* temp_surface{ nullptr };
	switch (text.mode_) {
		case FontRenderMode::SOLID:
			temp_surface = TTF_RenderText_Solid(font.get(), text.content_, text.color_);
			break;
		case FontRenderMode::SHADED:
			temp_surface = TTF_RenderText_Shaded(font.get(), text.content_, text.color_, text.background_shading_);
			break;
		case FontRenderMode::BLENDED:
			temp_surface = TTF_RenderText_Blended(font.get(), text.content_, text.color_);
			break;
		default:
			assert(!"Unrecognized render mode when creating surface for sdl text texture");
			break;
	}
	assert(temp_surface != nullptr && "Failed to load sdl text onto sdl surface");
	// Destroy old texture.
	auto& sdl_texture_manager{ GetSDLTextureManager() };
	sdl_texture_manager.SetTexture(text_key, sdl_texture_manager.CreateTextureFromSurface(temp_surface));
	TTF_SetFontStyle(font.get(), static_cast<int>(FontStyle::NORMAL));
	SDL_FreeSurface(temp_surface);
}

void SDLTextManager::SetSolidRenderMode(const std::size_t text_key) {
	auto it{ text_map_.find(text_key) };
	if (it != std::end(text_map_)) {
		auto& text{ it->second };
		text.mode_ = FontRenderMode::SOLID;
		RefreshText(text_key, text);
	}
}

void SDLTextManager::SetShadedRenderMode(const std::size_t text_key, const Color& background_shading) {
	auto it{ text_map_.find(text_key) };
	if (it != std::end(text_map_)) {
		auto& text{ it->second };
		text.background_shading_ = background_shading;
		text.mode_ = FontRenderMode::SHADED;
		RefreshText(text_key, text);
	}
}

void SDLTextManager::SetBlendedRenderMode(const std::size_t text_key) {
	auto it{ text_map_.find(text_key) };
	if (it != std::end(text_map_)) {
		auto& text{ it->second };
		text.mode_ = FontRenderMode::BLENDED;
		RefreshText(text_key, text);
	}
}

SDLTextManager& GetSDLTextManager() {
	static SDLTextManager default_text_manager;
	return default_text_manager;
}

} // namespace impl

namespace services {

interfaces::TextManager& GetTextManager() {
	return impl::GetSDLTextManager();
}

} // namespace services

} // namespace ptgn