#include "Text.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include "math/Math.h"
#include "texture/FontManager.h"

namespace ptgn {

namespace impl {

Text::Text(const char* text_content, std::size_t font_key, const Color& color) :
	content_{ text_content },
	font_key_{ font_key_ } {
	color_{ color },
	RefreshTexture();
}

Text::~Text() {
	auto& sdl_texture_manager{ GetSDLTextureManager() };
	sdl_texture_manager.UnloadTexture(texture_key_);
}

void Text::RefreshTexture() {
	auto& sdl_font_manager{ GetSDLFontManager() };
	auto font{ sdl_font_manager.GetFont(font_key_) };
	TTF_SetFontStyle(font.get(), style_);
	SDL_Surface* temp_surface{ nullptr };
	switch (mode_) {
		case FontRenderMode::SOLID:
			temp_surface = TTF_RenderText_Solid(font.get(), content_, color_);
			break;
		case FontRenderMode::SHADED:
			temp_surface = TTF_RenderText_Shaded(font.get(), content_, color_, background_shading_);
			break;
		case FontRenderMode::BLENDED:
			temp_surface = TTF_RenderText_Blended(font.get(), content_, color_);
			break;
		default:
			assert(!"Unrecognized render mode when creating surface for text texture");
			break;
	}
	assert(temp_surface != nullptr && "Failed to load sdl text onto sdl surface");
	// Destroy old texture.
	auto& sdl_texture_manager{ GetSDLTextureManager() };
	sdl_texture_manager.ResetTexture(texture_key_, )
	texture_ = ScreenRenderer::CreateTexture(temp_surface);
	TTF_SetFontStyle(font.get(), static_cast<int>(FontStyle::NORMAL));
	SDL_FreeSurface(temp_surface);
}

void Text::SetContent(const char* new_content) {
	content_ = new_content;
	RefreshTexture();
}

void Text::SetColor(const Color& new_color) {
	color_ = new_color;
	RefreshTexture();
}

void Text::SetFont(const char* new_font_name) {
	font_key_ = math::Hash(new_font_name);
	assert(FontManager::HasFont(font_key_) &&
		   "Cannot set text font which has not been loaded into FontManager");
	RefreshTexture();
}


void Text::SetSolidRenderMode() {
	mode_ = FontRenderMode::SOLID;
	RefreshTexture();
}

void Text::SetShadedRenderMode(const Color& background_shading) {
	shading_background_color_ = shading_background_color;
	mode_ = FontRenderMode::SHADED;
	RefreshTexture();
}

void Text::SetBlendedRenderMode() {
	mode_ = FontRenderMode::BLENDED;
	RefreshTexture();
}

} // namespace impl

} // namespace ptgn