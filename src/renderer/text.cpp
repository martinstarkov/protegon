#include "protegon/text.h"

#include "SDL.h"
#include "SDL_ttf.h"
#include "protegon/font.h"
#include "protegon/game.h"
#include "utility/debug.h"

namespace ptgn {

Texture Text::RecreateTexture() {
	PTGN_ASSERT(IsValid(), "Cannot recreate texture for text which is uninitialized or destroyed");
	PTGN_ASSERT(
		instance_->font_.IsValid(),
		"Cannot recreate texture for font which is uninitialized or destroyed"
	);

	if (instance_->content_.empty()) {
		return {}; // Skip creating texture for empty text.
	}

	const auto& f = instance_->font_.GetInstance();

	TTF_SetFontStyle(f.get(), static_cast<int>(instance_->font_style_));

	std::shared_ptr<SDL_Surface> surface;

	SDL_Color tc{ instance_->text_color_.r, instance_->text_color_.g, instance_->text_color_.b,
				  instance_->text_color_.a };

	switch (instance_->render_mode_) {
		case FontRenderMode::Solid:
			surface = std::shared_ptr<SDL_Surface>{
				TTF_RenderUTF8_Solid(f.get(), instance_->content_.c_str(), tc), SDL_FreeSurface
			};
			break;
		case FontRenderMode::Shaded: {
			SDL_Color sc{ instance_->shading_color_.r, instance_->shading_color_.g,
						  instance_->shading_color_.b, instance_->shading_color_.a };
			surface = std::shared_ptr<SDL_Surface>{
				TTF_RenderUTF8_Shaded(f.get(), instance_->content_.c_str(), tc, sc), SDL_FreeSurface
			};
			break;
		}
		case FontRenderMode::Blended:
			surface = std::shared_ptr<SDL_Surface>{
				TTF_RenderUTF8_Blended(f.get(), instance_->content_.c_str(), tc), SDL_FreeSurface
			};
			break;
		default: PTGN_ERROR("Unrecognized render mode given when creating text");
	}

	PTGN_ASSERT(surface != nullptr, "Failed to create surface for given text information");

	return Texture(surface);
}

Text::Text(
	const FontOrKey& font, const std::string& content, const Color& text_color,
	FontStyle font_style /*= FontStyle::Normal*/,
	FontRenderMode render_mode /*= FontRenderMode::Solid*/,
	const Color& shading_color /*= color::White*/
) {
	if (!IsValid()) {
		instance_ = std::make_shared<impl::TextInstance>();
	}
	instance_->font_		  = GetFont(font);
	instance_->content_		  = content;
	instance_->text_color_	  = text_color;
	instance_->font_style_	  = font_style;
	instance_->render_mode_	  = render_mode;
	instance_->shading_color_ = shading_color;
	instance_->texture_		  = RecreateTexture();
}

void Text::SetFont(const FontOrKey& font) {
	if (!IsValid()) {
		instance_ = std::make_shared<impl::TextInstance>();
	}
	auto f = GetFont(font);
	if (f.GetInstance() == instance_->font_.GetInstance()) {
		return;
	}
	instance_->font_	= f;
	instance_->texture_ = RecreateTexture();
}

void Text::SetContent(const std::string& content) {
	if (!IsValid()) {
		instance_ = std::make_shared<impl::TextInstance>();
	}
	if (content == instance_->content_) {
		return;
	}
	instance_->content_ = content;
	instance_->texture_ = RecreateTexture();
}

void Text::SetColor(const Color& text_color) {
	if (!IsValid()) {
		instance_ = std::make_shared<impl::TextInstance>();
	}
	if (text_color == instance_->text_color_) {
		return;
	}
	instance_->text_color_ = text_color;
	instance_->texture_	   = RecreateTexture();
}

void Text::SetFontStyle(FontStyle font_style) {
	if (!IsValid()) {
		instance_ = std::make_shared<impl::TextInstance>();
	}
	if (font_style == instance_->font_style_) {
		return;
	}
	instance_->font_style_ = font_style;
	instance_->texture_	   = RecreateTexture();
}

void Text::SetFontRenderMode(FontRenderMode render_mode) {
	if (!IsValid()) {
		instance_ = std::make_shared<impl::TextInstance>();
	}
	if (render_mode == instance_->render_mode_) {
		return;
	}
	instance_->render_mode_ = render_mode;
	instance_->texture_		= RecreateTexture();
}

void Text::SetShadingColor(const Color& shading_color) {
	if (!IsValid()) {
		instance_ = std::make_shared<impl::TextInstance>();
	}
	if (shading_color == instance_->shading_color_) {
		return;
	}
	instance_->render_mode_	  = FontRenderMode::Shaded;
	instance_->shading_color_ = shading_color;
	instance_->texture_		  = RecreateTexture();
}

const Font& Text::GetFont() const {
	PTGN_ASSERT(IsValid(), "Cannot get font of uninitialized or destroyed texture");
	return instance_->font_;
}

const std::string& Text::GetContent() const {
	PTGN_ASSERT(IsValid(), "Cannot get content of uninitialized or destroyed texture");
	return instance_->content_;
}

const Color& Text::GetColor() const {
	PTGN_ASSERT(IsValid(), "Cannot get color of uninitialized or destroyed texture");
	return instance_->text_color_;
}

FontStyle Text::GetFontStyle() const {
	PTGN_ASSERT(IsValid(), "Cannot get font style of uninitialized or destroyed texture");
	return instance_->font_style_;
}

FontRenderMode Text::GetFontRenderMode() const {
	PTGN_ASSERT(IsValid(), "Cannot get font render mode of uninitialized or destroyed texture");
	return instance_->render_mode_;
}

const Color& Text::GetShadingColor() const {
	PTGN_ASSERT(IsValid(), "Cannot get shading color of uninitialized or destroyed texture");
	return instance_->shading_color_;
}

Font Text::GetFont(const FontOrKey& font) {
	Font f;
	if (std::holds_alternative<std::size_t>(font)) {
		std::size_t font_key{ std::get<std::size_t>(font) };
		PTGN_ASSERT(game.font.Has(font_key), "game.font.Load() into manager before creating text");
		f = game.font.Get(font_key);
	} else {
		f = std::get<Font>(font);
	}
	PTGN_ASSERT(f.IsValid(), "Cannot create text with an invalid font");
	return f;
}

void Text::SetVisibility(bool visibility) {
	PTGN_ASSERT(IsValid(), "Cannot set visibility of text which is uninitialized or destroyed");
	instance_->visible_ = visibility;
}

bool Text::GetVisibility() const {
	PTGN_ASSERT(IsValid(), "Cannot get visibility of text which is uninitialized or destroyed");
	return instance_->visible_;
}

void Text::Draw(const Rectangle<int>& destination) const {
	if (!IsValid()) {
		return;
	}
	if (!instance_->visible_) {
		return;
	}
	if (instance_->content_.empty()) {
		return;
	}
	if (!instance_->texture_.IsValid()) {
		return;
	}
	game.renderer.DrawTexture(
		instance_->texture_, destination.pos, destination.size, {}, {}, destination.origin,
		Flip::None
	);
}

V2_int Text::GetSize(const FontOrKey& font, const std::string& content) {
	V2_int size;
	TTF_SizeUTF8(GetFont(font).GetInstance().get(), content.c_str(), &size.x, &size.y);
	return size;
}

} // namespace ptgn