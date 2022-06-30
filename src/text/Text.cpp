#include "Text.h"

#include "managers/FontManager.h"

#include <SDL.h>
#include <SDL_ttf.h>

namespace ptgn {

namespace internal {

Text::Text(const Renderer& renderer, const managers::id font_key, const char* content, const Color& color) :
	texture_{ renderer },
	font_key_{ font_key },
	content_{ content },
	color_{ color } {
	auto& font_manager{ GetFontManager() };
	assert(font_manager.Has(font_key) && "Must first load font into the font manager before loading text into the text manager");
	Refresh();
}

void Text::Refresh() {
	auto& font_manager{ GetFontManager() };
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
	auto& font_manager{ GetFontManager() };
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

void Text::Draw(const V2_int& text_position, const V2_int& text_size) const {
	assert(texture_ != nullptr && "Cannot draw text with non-existent texture");
	SDL_Rect destination{ text_position.x, text_position.y, text_size.x, text_size.y };
	SDL_RenderCopy(texture_.GetRenderer(), texture_, NULL, &destination);
}

} // namespace internal

} // namespace ptgn