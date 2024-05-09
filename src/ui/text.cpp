#include "protegon/text.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include "core/game.h"

// TODO: Figure out the font situation.

namespace ptgn {

Text::Text(std::size_t font_key, const char* content, const Color& color) :
	content_{ content },
	color_{ color } {
	auto& font_manager{ global::GetGame().managers.font };
	assert(font_manager.Has(font_key) && "font::Load() into manager before creating text");
	font_ = *font_manager.Get(font_key);
	assert(font_.IsValid() && "Attempting to create text from invalid font?");
	Refresh();
}

Text::Text(const Font& font, const char* content, const Color& color) :
	font_{ font },
	content_{ content },
	color_{ color } {
	assert(font_.IsValid() && "Attempting to create text from invalid font?");
	Refresh();
}

void Text::Refresh() {
	assert(font_.IsValid() && "Cannot refresh text due to invalid font");
	auto font{ font_.font_.get() };
	TTF_SetFontStyle(font, static_cast<int>(style_));
	SDL_Surface* temp_surface{ nullptr };
	switch (mode_) {
		case Font::RenderMode::SOLID:
			temp_surface = TTF_RenderText_Solid(font, content_, color_);
			break;
		case Font::RenderMode::SHADED:
			temp_surface = TTF_RenderText_Shaded(font, content_, color_, bg_shading_);
			break;
		case Font::RenderMode::BLENDED:
			temp_surface = TTF_RenderText_Blended(font, content_, color_);
			break;
		default:
			assert(!"Unrecognized render mode when creating surface for the text texture");
			break;
	}
	assert(temp_surface != nullptr && "Failed to load text onto surface");
	texture_ = Texture{ temp_surface };
	assert(IsValid() && "Failed to create text");
	TTF_SetFontStyle(font, static_cast<int>(Font::Style::NORMAL));
}

void Text::SetContent(const char* new_content) {
	content_ = new_content;
	Refresh();
}
void Text::SetColor(const Color& new_color) {
	color_ = new_color;
	Refresh();
}

void Text::SetFont(const Font& new_font) {
	font_ = new_font;
	Refresh();
}

void Text::SetSolidRenderMode() {
	mode_ = Font::RenderMode::SOLID;
	Refresh();
}

void Text::SetShadedRenderMode(const Color& bg_shading) {
	bg_shading_ = bg_shading;
	mode_ = Font::RenderMode::SHADED;
	Refresh();
}

void Text::SetBlendedRenderMode() {
	mode_ = Font::RenderMode::BLENDED;
	Refresh();
}

bool Text::IsValid() const {
	return texture_.IsValid() && font_.IsValid();
}

void Text::Draw(const Rectangle<int>& box) const {
	SDL_Rect destination{ box.pos.x, box.pos.y, box.size.x, box.size.y };
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	assert(renderer != nullptr && "Game instance destroyed or nonexistent?");
	assert(texture_.IsValid() && "Text texture destroyed?");
	assert(font_.IsValid() && "Text font destroyed?");
	SDL_RenderCopy(renderer, texture_.texture_.get(), NULL, &destination);
}

} // namespace ptgn