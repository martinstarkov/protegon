#include "protegon/text.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

#include "core/manager.h"
#include "protegon/color.h"
#include "protegon/font.h"
#include "protegon/game.h"
#include "protegon/polygon.h"
#include "protegon/surface.h"
#include "protegon/texture.h"
#include "protegon/vector2.h"
#include "renderer/flip.h"
#include "renderer/renderer.h"
#include "utility/debug.h"
#include "utility/handle.h"

namespace ptgn {

Texture Text::RecreateTexture() {
	auto& t{ Get() };
	PTGN_ASSERT(
		t.font_.IsValid(), "Cannot recreate texture for font which is uninitialized or destroyed"
	);

	if (t.content_.empty()) {
		return {}; // Skip creating texture for empty text.
	}

	Surface surface{ t.font_,	 t.font_style_,	   t.text_color_,		t.render_mode_,
					 t.content_, t.shading_color_, t.wrap_after_pixels_ };

	return Texture(surface);
}

Text::Text(
	const FontOrKey& font, const std::string_view& content, const Color& text_color,
	FontStyle font_style /*= FontStyle::Normal*/,
	FontRenderMode render_mode /*= FontRenderMode::Solid*/,
	const Color& shading_color /*= color::White*/
) {
	Create();
	auto& t{ Get() };
	t.font_			 = GetFont(font);
	t.content_		 = content;
	t.text_color_	 = text_color;
	t.font_style_	 = font_style;
	t.render_mode_	 = render_mode;
	t.shading_color_ = shading_color;
	t.texture_		 = RecreateTexture();
}

void Text::SetFont(const FontOrKey& font) {
	Create();
	auto f = GetFont(font);
	auto& t{ Get() };
	if (f == t.font_) {
		return;
	}
	t.font_	   = f;
	t.texture_ = RecreateTexture();
}

void Text::SetContent(const std::string_view& content) {
	Create();
	auto& t{ Get() };
	if (content == t.content_) {
		return;
	}
	t.content_ = content;
	t.texture_ = RecreateTexture();
}

void Text::SetWrapAfter(std::uint32_t wrap_after_pixels) {
	Create();
	auto& t{ Get() };
	if (wrap_after_pixels == t.wrap_after_pixels_) {
		return;
	}
	t.wrap_after_pixels_ = wrap_after_pixels;
	t.texture_			 = RecreateTexture();
}

void Text::SetColor(const Color& text_color) {
	Create();
	auto& t{ Get() };
	if (text_color == t.text_color_) {
		return;
	}
	t.text_color_ = text_color;
	t.texture_	  = RecreateTexture();
}

void Text::SetFontStyle(FontStyle font_style) {
	Create();
	auto& t{ Get() };
	if (font_style == t.font_style_) {
		return;
	}
	t.font_style_ = font_style;
	t.texture_	  = RecreateTexture();
}

void Text::SetFontRenderMode(FontRenderMode render_mode) {
	Create();
	auto& t{ Get() };
	if (render_mode == t.render_mode_) {
		return;
	}
	t.render_mode_ = render_mode;
	t.texture_	   = RecreateTexture();
}

void Text::SetShadingColor(const Color& shading_color) {
	Create();
	auto& t{ Get() };
	if (shading_color == t.shading_color_) {
		return;
	}
	t.render_mode_	 = FontRenderMode::Shaded;
	t.shading_color_ = shading_color;
	t.texture_		 = RecreateTexture();
}

const Font& Text::GetFont() const {
	return Get().font_;
}

std::string_view Text::GetContent() const {
	return Get().content_;
}

const Color& Text::GetColor() const {
	return Get().text_color_;
}

FontStyle Text::GetFontStyle() const {
	return Get().font_style_;
}

FontRenderMode Text::GetFontRenderMode() const {
	return Get().render_mode_;
}

const Color& Text::GetShadingColor() const {
	return Get().shading_color_;
}

const Texture& Text::GetTexture() const {
	return Get().texture_;
}

Font Text::GetFont(const FontOrKey& font) {
	Font f;
	if (std::holds_alternative<impl::FontManager::Key>(font)) {
		const auto& font_key{ std::get<impl::FontManager::Key>(font) };
		PTGN_ASSERT(game.font.Has(font_key), "game.font.Load() into manager before creating text");
		f = game.font.Get(font_key);
	} else if (std::holds_alternative<impl::FontManager::InternalKey>(font)) {
		const auto& font_key{ std::get<impl::FontManager::InternalKey>(font) };
		PTGN_ASSERT(game.font.Has(font_key), "game.font.Load() into manager before creating text");
		f = game.font.Get(font_key);
	} else if (std::holds_alternative<Font>(font)) {
		f = std::get<Font>(font);
	}
	PTGN_ASSERT(f.IsValid(), "Cannot create text with an invalid font");
	return f;
}

void Text::SetVisibility(bool visibility) {
	Get().visible_ = visibility;
}

bool Text::GetVisibility() const {
	return Get().visible_;
}

void Text::Draw(const Rectangle<float>& destination, float z_index) const {
	if (!IsValid()) {
		return;
	}
	auto& t{ Get() };
	if (!t.visible_) {
		return;
	}
	if (t.content_.empty()) {
		return;
	}
	if (!t.texture_.IsValid()) {
		return;
	}
	game.draw.Texture(
		t.texture_, destination.pos, destination.size,
		{ {}, {}, destination.origin, Flip::None, 0.0f, {}, z_index }
	);
}

V2_int Text::GetSize() const {
	return GetSize(GetFont(), std::string(GetContent()));
}

V2_int Text::GetSize(const FontOrKey& font, const std::string& content) {
	return Surface::GetSize(GetFont(font), content);
}

} // namespace ptgn