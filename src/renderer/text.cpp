#include "renderer/text.h"

#include <cstdint>
#include <string>
#include <variant>

#include "core/game.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/font.h"
#include "renderer/surface.h"
#include "renderer/texture.h"
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

	Surface surface{ t.font_,			t.font_style_, t.text_color_,	 t.render_mode_,
					 t.content_,		t.point_size_, t.shading_color_, t.wrap_after_pixels_,
					 t.wrap_alignment_, t.line_skip_ };

	return Texture(surface);
}

Text::Text(const std::string_view& content, const Color& text_color, const FontOrKey& font) {
	Create();
	auto& t{ Get() };

	Font f;

	if (font == FontOrKey{}) {
		f = game.font.GetDefault();
	} else {
		f = game.font.GetFontOrKey(font);
	}

	PTGN_ASSERT(f.IsValid(), "Cannot create text with invalid font");

	t.font_		  = f;
	t.content_	  = content;
	t.text_color_ = text_color;
	t.texture_	  = RecreateTexture();
}

void Text::Draw(const Rect& destination, std::int32_t render_layer) const {
	if (!IsValid()) {
		return;
	}
	if (!GetVisibility()) {
		return;
	}
	if (GetColor().a == 0) {
		return;
	}
	if (GetContent().empty()) {
		return;
	}

	const ptgn::Texture& texture{ GetTexture() };

	if (!texture.IsValid()) {
		return;
	}

	Rect dest{ destination };

	if (dest.size.IsZero()) {
		dest.size = GetSize();
	}

	texture.Draw(dest, {}, render_layer);
}

Text& Text::SetFont(const FontOrKey& font) {
	Create();

	Font f;

	if (font == FontOrKey{}) {
		f = game.font.GetDefault();
	} else {
		f = game.font.GetFontOrKey(font);
	}

	auto& t{ Get() };
	if (f == t.font_) {
		return *this;
	}

	PTGN_ASSERT(f.IsValid(), "Cannot set text font to be invalid");

	t.font_	   = f;
	t.texture_ = RecreateTexture();
	return *this;
}

Text& Text::SetContent(const std::string_view& content) {
	Create();
	auto& t{ Get() };
	if (content == t.content_) {
		return *this;
	}
	t.content_ = content;
	t.texture_ = RecreateTexture();
	return *this;
}

Text& Text::SetWrapAfter(std::uint32_t wrap_after_pixels) {
	Create();
	auto& t{ Get() };
	if (wrap_after_pixels == t.wrap_after_pixels_) {
		return *this;
	}
	t.wrap_after_pixels_ = wrap_after_pixels;
	t.texture_			 = RecreateTexture();
	return *this;
}

Text& Text::SetLineSkip(std::int32_t line_skip) {
	Create();
	auto& t{ Get() };
	if (line_skip == t.line_skip_) {
		return *this;
	}
	t.line_skip_ = line_skip;
	t.texture_	 = RecreateTexture();
	return *this;
}

Text& Text::SetSize(std::int32_t point_size) {
	Create();
	auto& t{ Get() };
	if (point_size == t.point_size_) {
		return *this;
	}
	t.point_size_ = point_size;
	t.texture_	  = RecreateTexture();
	return *this;
}

Text& Text::SetWrapAlignment(TextWrapAlignment wrap_alignment) {
	Create();
	auto& t{ Get() };
	if (wrap_alignment == t.wrap_alignment_) {
		return *this;
	}
	t.wrap_alignment_ = wrap_alignment;
	t.texture_		  = RecreateTexture();
	return *this;
}

Text& Text::SetColor(const Color& text_color) {
	Create();
	auto& t{ Get() };
	if (text_color == t.text_color_) {
		return *this;
	}
	t.text_color_ = text_color;
	t.texture_	  = RecreateTexture();
	return *this;
}

Text& Text::SetFontStyle(FontStyle font_style) {
	Create();
	auto& t{ Get() };
	if (font_style == t.font_style_) {
		return *this;
	}
	t.font_style_ = font_style;
	t.texture_	  = RecreateTexture();
	return *this;
}

Text& Text::SetFontRenderMode(FontRenderMode render_mode) {
	Create();
	auto& t{ Get() };
	if (render_mode == t.render_mode_) {
		return *this;
	}
	t.render_mode_ = render_mode;
	t.texture_	   = RecreateTexture();
	return *this;
}

Text& Text::SetShadingColor(const Color& shading_color) {
	Create();
	auto& t{ Get() };
	if (shading_color == t.shading_color_) {
		return *this;
	}
	t.render_mode_	 = FontRenderMode::Shaded;
	t.shading_color_ = shading_color;
	t.texture_		 = RecreateTexture();
	return *this;
}

Text& Text::SetVisibility(bool visibility) {
	Get().visible_ = visibility;
	return *this;
}

Text& Text::ToggleVisibility() {
	auto& t{ Get() };
	t.visible_ = !t.visible_;
	return *this;
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

bool Text::GetVisibility() const {
	return Get().visible_;
}

const Texture& Text::GetTexture() const {
	return Get().texture_;
}

V2_int Text::GetSize() const {
	return Get().texture_.GetSize();
}

V2_int Text::GetSize(const FontOrKey& font, const std::string& content) {
	Font f{ game.font.GetFontOrKey(font) };
	PTGN_ASSERT(f.IsValid(), "Cannot get size of text with invalid font");
	return Surface::GetSize(f, content);
}

} // namespace ptgn