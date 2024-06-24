#include "protegon/text.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include "core/game.h"
#include "protegon/debug.h"
#include "protegon/font.h"

namespace ptgn {

namespace impl {

TextInstance::TextInstance(
	const Font& font, const std::string& content, const Color& text_color, FontStyle font_style,
	FontRenderMode render_mode, const Color& shading_color
) :
	font_{ font },
	content_{ content },
	text_color_{ text_color },
	font_style_{ font_style },
	render_mode_{ render_mode },
	shading_color_{ shading_color } {
	RecreateTexture();
}

void TextInstance::RecreateTexture() {
	PTGN_ASSERT(font_.IsValid(), "Cannot create text which has uninitialized or destroyed font");

	if (content_ == "") {
		return; // Skip creating texture for empty text.
	}

	const auto& f = font_.GetInstance();

	TTF_SetFontStyle(f.get(), static_cast<int>(font_style_));

	std::shared_ptr<SDL_Surface> surface;

	switch (render_mode_) {
		case FontRenderMode::Solid:
			surface = std::shared_ptr<SDL_Surface>{
				TTF_RenderText_Solid(f.get(), content_.c_str(), text_color_), SDL_FreeSurface
			};
			break;
		case FontRenderMode::Shaded:
			surface = std::shared_ptr<SDL_Surface>{
				TTF_RenderText_Shaded(f.get(), content_.c_str(), text_color_, shading_color_),
				SDL_FreeSurface
			};
			break;
		case FontRenderMode::Blended:
			surface = std::shared_ptr<SDL_Surface>{
				TTF_RenderText_Blended(f.get(), content_.c_str(), text_color_), SDL_FreeSurface
			};
			break;
		default: PTGN_ASSERT(false, "Unrecognized render mode given when creating text");
	}

	texture_ = std::shared_ptr<SDL_Texture>{
		SDL_CreateTextureFromSurface(global::GetGame().sdl.GetRenderer().get(), surface.get()),
		SDL_DestroyTexture
	};
}

} // namespace impl

Text::Text(
	const FontOrKey& font, const std::string& content, const Color& text_color,
	FontStyle font_style, FontRenderMode render_mode, const Color& shading_color
) {
	instance_ = std::make_shared<impl::TextInstance>(
		GetFont(font), content, text_color, font_style, render_mode, shading_color
	);
}

void Text::SetFont(const FontOrKey& font) {
	PTGN_CHECK(instance_ != nullptr, "Cannot set font of uninitialized or destroyed texture");
	instance_->font_ = GetFont(font);
	instance_->RecreateTexture();
}

void Text::SetContent(const std::string& content) {
	instance_->content_ = content;
	instance_->RecreateTexture();
}

void Text::SetColor(const Color& text_color) {
	instance_->text_color_ = text_color;
	instance_->RecreateTexture();
}

void Text::SetFontStyle(FontStyle font_style) {
	instance_->font_style_ = font_style;
	instance_->RecreateTexture();
}

void Text::SetFontRenderMode(FontRenderMode render_mode) {
	instance_->render_mode_ = render_mode;
	instance_->RecreateTexture();
}

void Text::SetShadingColor(const Color& shading_color) {
	instance_->render_mode_	  = FontRenderMode::Shaded;
	instance_->shading_color_ = shading_color;
	instance_->RecreateTexture();
}

const Font& Text::GetFont() const {
	PTGN_CHECK(instance_ != nullptr, "Cannot get font of uninitialized or destroyed texture");
	return instance_->font_;
}

const std::string& Text::GetContent() const {
	PTGN_CHECK(instance_ != nullptr, "Cannot get content of uninitialized or destroyed texture");
	return instance_->content_;
}

const Color Text::GetColor() const {
	PTGN_CHECK(instance_ != nullptr, "Cannot get color of uninitialized or destroyed texture");
	return instance_->text_color_;
}

FontStyle Text::GetFontStyle() const {
	PTGN_CHECK(instance_ != nullptr, "Cannot get font style of uninitialized or destroyed texture");
	return instance_->font_style_;
}

FontRenderMode Text::GetFontRenderMode() const {
	PTGN_CHECK(
		instance_ != nullptr, "Cannot get font render mode of uninitialized or destroyed texture"
	);
	return instance_->render_mode_;
}

const Color Text::GetShadingColor() const {
	PTGN_CHECK(
		instance_ != nullptr, "Cannot get shading color of uninitialized or destroyed texture"
	);
	return instance_->shading_color_;
}

Font Text::GetFont(const FontOrKey& font) {
	Font f;
	if (std::holds_alternative<std::size_t>(font)) {
		std::size_t font_key{ std::get<std::size_t>(font) };
		auto& fonts{ global::GetGame().managers.font };
		PTGN_CHECK(fonts.Has(font_key), "font::Load() into manager before creating text");
		f = fonts.Get(font_key);
	} else {
		f = std::get<Font>(font);
	}
	PTGN_CHECK(f.IsValid(), "Cannot create text with an invalid font");
	return f;
}

void Text::SetVisibility(bool visibility) {
	PTGN_CHECK(IsValid(), "Cannot set visibility of text which is uninitialized or destroyed");
	instance_->visible_ = visibility;
}

bool Text::GetVisibility() const {
	PTGN_CHECK(IsValid(), "Cannot get visibility of text which is uninitialized or destroyed");
	return instance_->visible_;
}

void Text::Draw(const Rectangle<int>& destination) const {
	if (!IsValid()) {
		return;
	}
	if (!instance_->visible_) {
		return;
	}
	if (instance_->texture_ == nullptr) {
		return;
	}
	SDL_Rect dest(destination);
	SDL_RenderCopy(
		global::GetGame().sdl.GetRenderer().get(), instance_->texture_.get(), NULL, &dest
	);
}

V2_int Text::GetSize(const FontOrKey& font, const std::string& content) {
	V2_int size;
	TTF_SizeUTF8(GetFont(font).GetInstance().get(), content.c_str(), &size.x, &size.y);
	return size;
}

} // namespace ptgn