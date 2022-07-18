#include "Text.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include "managers/TextureManager.h"

namespace ptgn {

namespace internal {

Text::Text(const managers::id font_key, const char* content, const Color& color) :
	font_key_{ font_key },
	content_{ content },
	color_{ color } {
	auto& font_manager{ managers::GetManager<managers::FontManager>() };
	assert(font_manager.Has(font_key) && "Must first load font into the font manager before loading text into the text manager");
	Refresh();
}

void Text::Refresh() {
	auto& font_manager{ managers::GetManager<managers::FontManager>() };
	auto& font{ *font_manager.Get(font_key_) };
	assert(font != nullptr && "Cannot refresh text for font which is not loaded in the the font manager");
	TTF_SetFontStyle(font, style_);
	SDL_Surface* temp_surface{ nullptr };
	switch (mode_) {
		case FontRenderMode::SOLID:
			temp_surface = TTF_RenderText_Solid(font, content_, color_);
			break;
		case FontRenderMode::SHADED:
			temp_surface = TTF_RenderText_Shaded(font, content_, color_, background_shading_);
			break;
		case FontRenderMode::BLENDED:
			temp_surface = TTF_RenderText_Blended(font, content_, color_);
			break;
		default:
			assert(!"Unrecognized render mode when creating surface for the text texture");
			break;
	}
	assert(temp_surface != nullptr && "Failed to load text onto the surface");
	texture_.Reset(temp_surface);
	TTF_SetFontStyle(font, static_cast<int>(FontStyle::NORMAL));
}

void Text::SetContent(const char* new_content) {
	content_ = new_content;
	Refresh();
}

void Text::SetColor(const Color& new_color) {
	color_ = new_color;
	Refresh();
}

void Text::SetFont(const managers::id new_font_key) {
	auto& font_manager{ managers::GetManager<managers::FontManager>() };
	assert(font_manager.Has(new_font_key) && "Cannot set text font to a font which has not been loaded into the font manager");
	font_key_ = new_font_key;
	Refresh();
}

void Text::SetSolidRenderMode() {
	mode_ = FontRenderMode::SOLID;
	Refresh();
}

void Text::SetShadedRenderMode(const Color& background_shading) {
	background_shading_ = background_shading;
	mode_ = FontRenderMode::SHADED;
	Refresh();
}

void Text::SetBlendedRenderMode() {
	mode_ = FontRenderMode::BLENDED;
	Refresh();
}

Texture Text::GetTexture() const {
	assert(texture_ != nullptr && "Cannot return nullptr texture for text");
	return texture_;
}

} // namespace internal

} // namespace ptgn