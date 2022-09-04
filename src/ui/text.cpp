#include "protegon/text.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include "core/game.h"

// TODO: Figure out the font situation.

namespace ptgn {

Text::Text(const char* font_key, const char* text_content, const Color& text_color) :
	font_key_{ math::Hash(font_key) },
	content_{ text_content },
	color_{ text_color } {
	auto& font_manager{ manager::Get<FontManager>() };
	assert(font_manager.Has(font_key_) && "Must first load font into the font manager before loading text into the text manager");
	Refresh();
}

Text::~Text() {
	// TODO: Make sure this is right?
}

void Text::Refresh() {
	auto& font_manager{ manager::Get<FontManager>() };
	auto font{ font_manager.Get(font_key_) };
	assert(font != nullptr && "Cannot refresh text for font which is not loaded in the the font manager");
	TTF_SetFontStyle(*font, static_cast<int>(style_));
	SDL_Surface* temp_surface{ nullptr };
	switch (mode_) {
		case FontRenderMode::SOLID:
			temp_surface = TTF_RenderText_Solid(*font, content_, color_);
			break;
		case FontRenderMode::SHADED:
			temp_surface = TTF_RenderText_Shaded(*font, content_, color_, background_shading_);
			break;
		case FontRenderMode::BLENDED:
			temp_surface = TTF_RenderText_Blended(*font, content_, color_);
			break;
		default:
			assert(!"Unrecognized render mode when creating surface for the text texture");
			break;
	}
	assert(temp_surface != nullptr && "Failed to load text onto the surface");
	auto& texture_manager{ manager::Get<TextureManager>() };
	texture_manager.Load(texture_key_, temp_surface);
	TTF_SetFontStyle(*font, static_cast<int>(FontStyle::NORMAL));
}

void Text::SetFont(const char* new_font_key) {
	auto font_key{ math::Hash(new_font_key) };
	auto& font_manager{ manager::Get<FontManager>() };
	assert(font_manager.Has(font_key) && "Cannot set text font to a font which has not been loaded into the font manager");
	font_key_ = font_key;
	Refresh();
}

void Text::Draw(const Rectangle<int>& box) const {
	SDL_Rect destination{ box.position.x, box.position.y, box.size.x, box.size.y };
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	assert(renderer != nullptr && "Game instance destroyed or nonexistent?");
	SDL_RenderCopy(renderer, texture_.texture_, NULL, &destination);
}

} // namespace ptgn