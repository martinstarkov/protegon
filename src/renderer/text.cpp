#include "protegon/text.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include "protegon/debug.h"
#include "core/game.h"
#include "protegon/font.h"

namespace ptgn {

namespace impl {

TextInstance::TextInstance(const Texture& texture) : texture_{ texture } {}

} // namespace impl

Text::Text(
	const FontOrKey& font,
	const std::string& content,
	const Color& text_color,
	FontStyle font_style,
	FontRenderMode render_mode,
	const Color& shading_color
) {
	instance_ = std::make_shared<impl::TextInstance>(
		CreateTexture(GetFont(font), content, text_color, font_style, render_mode, shading_color)
	);
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

Texture Text::CreateTexture(
	const Font& font,
	const std::string& content,
	const Color& text_color,
	FontStyle font_style,
	FontRenderMode render_mode,
	const Color& shading_color
) {
	PTGN_ASSERT(font.IsValid(), "Cannot create text which has uninitialized or destroyed font");
	
	if (content == "") return {}; // Create null texture for empty text.
	
	const auto& f = font.GetInstance();

	TTF_SetFontStyle(f.get(), static_cast<int>(font_style));

	switch (render_mode) {
		case FontRenderMode::Solid:
			return { std::shared_ptr<SDL_Surface>{
				TTF_RenderText_Solid(f.get(), content.c_str(), text_color), SDL_FreeSurface } };
		case FontRenderMode::Shaded:
			return { std::shared_ptr<SDL_Surface>{
				TTF_RenderText_Shaded(f.get(), content.c_str(), text_color, shading_color), SDL_FreeSurface } };
		case FontRenderMode::Blended:
			return { std::shared_ptr<SDL_Surface>{
				TTF_RenderText_Blended(f.get(), content.c_str(), text_color), SDL_FreeSurface } };
		default:
			PTGN_ASSERT(false, "Unrecognized render mode given when creating text");
	}
	return {};
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
	if (!IsValid()) return;
	if (!instance_->visible_) return;
	if (!instance_->texture_.IsValid()) return;
	renderer::DrawTexture(instance_->texture_, destination);
}

V2_int Text::GetSize(const FontOrKey& font, const std::string& content) {
	V2_int size;
	TTF_SizeUTF8(GetFont(font).GetInstance().get(), content.c_str(), &size.x, &size.y);
	return size;
}

} // namespace ptgn