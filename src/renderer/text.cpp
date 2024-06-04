#include "protegon/text.h"

#include <cassert>

#include <SDL.h>
#include <SDL_ttf.h>

#include "core/game.h"
#include "protegon/font.h"

namespace ptgn {

namespace impl {

TextInstance::TextInstance(const Font& font, const std::string& content, const Color& color) :
	font_{ font }, content_{ content }, color_{ color } {}

} // namespace impl

Text::Text(const FontOrKey& font, const std::string& content, const Color& color) {
	instance_ = std::make_shared<impl::TextInstance>(GetFont(font), content, color);
	Refresh();
}

Font Text::GetFont(const FontOrKey& font) {
	Font f;
	if (std::holds_alternative<std::size_t>(font)) {
		std::size_t font_key{ std::get<std::size_t>(font) };
		auto& fonts{ global::GetGame().managers.font };
		assert(fonts.Has(font_key) && "font::Load() into manager before creating text");
		f = fonts.Get(font_key);
	} else {
		f = std::get<Font>(font);
	}
	assert(f.IsValid() && "Cannot create text with an invalid font");
	return f;
}

void Text::Refresh() {
	assert(IsValid() && "Cannot refresh texture which is uninitialized or destroyed");
	assert(instance_->font_.IsValid() && "Cannot refresh text which has uninitialized or destroyed font");
	
	if (instance_->content_ == "") return; // Cannot create surface for text with empty content.
	
	std::shared_ptr<TTF_Font>& font{ instance_->font_.instance_ };
	TTF_SetFontStyle(font.get(), static_cast<int>(instance_->style_));

	std::shared_ptr<SDL_Surface> surface{ nullptr, SDL_FreeSurface };
	
	switch (instance_->mode_) {
		case Font::RenderMode::SOLID:
			surface = std::shared_ptr<SDL_Surface>{
				TTF_RenderText_Solid(font.get(), instance_->content_.c_str(), instance_->color_),
				SDL_FreeSurface
			};
			break;
		case Font::RenderMode::SHADED:
			surface = std::shared_ptr<SDL_Surface>{
				TTF_RenderText_Shaded(font.get(), instance_->content_.c_str(), instance_->color_, instance_->bg_shading_),
				SDL_FreeSurface
			};
			break;
		case Font::RenderMode::BLENDED:
			surface = std::shared_ptr<SDL_Surface>{
				TTF_RenderText_Blended(font.get(), instance_->content_.c_str(), instance_->color_),
				SDL_FreeSurface
			};
			break;
		default:
			assert(!"Unrecognized render mode when creating surface for the text texture");
			break;
	}
	assert(surface != nullptr && "Failed to load text onto surface");

	instance_->texture_ = Texture{ surface };
	assert(instance_->texture_.IsValid() && "Failed to create text from texture");

	// TODO: Consider if this is necessary.
	//TTF_SetFontStyle(font.get(), static_cast<int>(Font::Style::NORMAL));
}

void Text::SetVisibility(bool visibility) {
	assert(IsValid() && "Cannot set visibility of text which is uninitialized or destroyed");
	instance_->visible_ = visibility;
}

bool Text::GetVisibility() const {
	assert(IsValid() && "Cannot get visibility of text which is uninitialized or destroyed");
	return instance_->visible_;
}

void Text::SetContent(const std::string& new_content) {
	assert(IsValid() && "Cannot set content of text which is uninitialized or destroyed");
	instance_->content_ = new_content;
	Refresh();
}
void Text::SetColor(const Color& new_color) {
	assert(IsValid() && "Cannot set color of text which is uninitialized or destroyed");
	instance_->color_ = new_color;
	Refresh();
}

void Text::SetFont(const FontOrKey& new_font) {
	assert(IsValid() && "Cannot set font of text which is uninitialized or destroyed");
	instance_->font_ = GetFont(new_font);
	Refresh();
}

void Text::SetSolidRenderMode() {
	assert(IsValid() && "Cannot set render mode of text which is uninitialized or destroyed");
	instance_->mode_ = Font::RenderMode::SOLID;
	Refresh();
}

void Text::SetShadedRenderMode(const Color& bg_shading) {
	assert(IsValid() && "Cannot set render mode of text which is uninitialized or destroyed");
	instance_->bg_shading_ = bg_shading;
	instance_->mode_ = Font::RenderMode::SHADED;
	Refresh();
}

void Text::SetBlendedRenderMode() {
	assert(IsValid() && "Cannot set render mode of text which is uninitialized or destroyed");
	instance_->mode_ = Font::RenderMode::BLENDED;
	Refresh();
}

void Text::Draw(const Rectangle<float>& box) const {
	assert(IsValid() && "Cannot draw text which is uninitialized or destroyed");
	
	if (!instance_->visible_) return;
	if (instance_->content_ == "") return; // Cannot draw text with empty content.
	
	SDL_Rect destination{
		static_cast<int>(box.pos.x),
		static_cast<int>(box.pos.y),
		static_cast<int>(box.size.x),
		static_cast<int>(box.size.y)
	};

	assert(instance_->texture_.IsValid() && "Cannot draw text which has an uninitialized or destroyed texture");
	
	SDL_RenderCopy(
		global::GetGame().sdl.GetRenderer().get(),
		instance_->texture_.instance_.get(),
		NULL, &destination
	);
}

} // namespace ptgn