#include "renderer/text.h"

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

#include "SDL_pixels.h"
#include "SDL_surface.h"
#include "SDL_ttf.h"
#include "core/game.h"
#include "core/game_object.h"
#include "ecs/ecs.h"
#include "math/hash.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/font.h"
#include "renderer/texture.h"
#include "utility/assert.h"
#include "utility/log.h"

namespace ptgn {

Text::Text(ecs::Manager& manager) : GameObject{ manager } {
	SetVisible(true);
}

Text::Text(
	ecs::Manager& manager, std::string_view content, const Color& text_color,
	std::string_view font_key
) :
	Text{ manager, TextContent{ content }, TextColor{ text_color }, FontKey{ font_key } } {}

Text::Text(
	ecs::Manager& manager, const TextContent& content, const TextColor& text_color,
	const FontKey& font_key
) :
	Text{ manager } {
	Add<TextContent>(content);
	if (text_color != TextColor{}) {
		Add<TextColor>(text_color);
	}
	if (font_key != FontKey{}) {
		Add<FontKey>(font_key);
	}
	RecreateTexture();
}

Text& Text::SetFont(std::string_view font_key) {
	return SetParameter(FontKey{ Hash(font_key) });
}

Text& Text::SetContent(std::string_view content) {
	return SetParameter(TextContent{ content });
}

Text& Text::SetColor(const Color& color) {
	return SetParameter(TextColor{ color });
}

Text& Text::SetFontStyle(FontStyle font_style) {
	return SetParameter(font_style);
}

Text& Text::SetFontSize(std::int32_t pixels) {
	return SetParameter(FontSize{ pixels });
}

Text& Text::SetFontRenderMode(FontRenderMode render_mode) {
	return SetParameter(render_mode);
}

Text& Text::SetShadingColor(const Color& shading_color) {
	SetParameter(FontRenderMode::Shaded);
	return SetParameter(TextShadingColor{ shading_color });
}

Text& Text::SetWrapAfter(std::uint32_t pixels) {
	return SetParameter(TextWrapAfter{ pixels });
}

Text& Text::SetLineSkip(std::int32_t pixels) {
	return SetParameter(TextLineSkip{ pixels });
}

Text& Text::SetTextJustify(TextJustify text_justify) {
	return SetParameter(text_justify);
}

std::size_t Text::GetFontKey() const {
	return GetParameter(FontKey{});
}

std::string_view Text::GetContent() const {
	return GetParameter(TextContent{});
}

Color Text::GetColor() const {
	return GetParameter(TextColor{});
}

FontStyle Text::GetFontStyle() const {
	return GetParameter(FontStyle{});
}

FontRenderMode Text::GetFontRenderMode() const {
	return GetParameter(FontRenderMode{});
}

Color Text::GetShadingColor() const {
	return GetParameter(TextShadingColor{});
}

TextJustify Text::GetTextJustify() const {
	return GetParameter(TextJustify{});
}

const impl::Texture& Text::GetTexture() const {
	PTGN_ASSERT(Has<impl::Texture>(), "Cannot retrieve text texture before it has been set");
	return Get<impl::Texture>();
}

std::int32_t Text::GetFontSize() const {
	FontSize font_size{ GetParameter(FontSize{}) };
	if (font_size == std::numeric_limits<std::int32_t>::infinity()) {
		auto font_key{ GetFontKey() };
		PTGN_ASSERT(
			game.font.Has(font_key),
			"Cannot get size of text font unless it is loaded in the font manager"
		);
		return game.font.GetHeight(font_key);
	}
	return font_size;
}

V2_int Text::GetSize() const {
	auto font_key{ GetFontKey() };
	PTGN_ASSERT(
		game.font.Has(font_key),
		"Cannot get size of text texture unless its font is loaded in the font manager"
	);
	return game.font.GetSize(font_key, std::string(GetContent()));
}

void Text::RecreateTexture() {
	impl::Texture& texture{ Has<impl::Texture>() ? Get<impl::Texture>() : Add<impl::Texture>() };

	std::string content{ GetParameter(TextContent{}) };

	if (content.empty()) {
		// Skip creating texture for empty text.
		texture = {};
		return;
	}

	FontKey font_key{ GetParameter(FontKey{}) };

	PTGN_ASSERT(
		game.font.Has(font_key),
		"Cannot create texture for text with font key which is not loaded in the font manager"
	);

	auto font{ game.font.Get(font_key) };

	PTGN_ASSERT(font != nullptr, "Cannot create texture for text with nullptr font");

	TTF_SetFontStyle(font, static_cast<int>(GetParameter(FontStyle{})));

	TTF_SetFontWrappedAlign(font, static_cast<int>(GetParameter(TextJustify{})));

#ifndef __EMSCRIPTEN__ // TODO: Re-enable this for Emscripten once it is supported (SDL_ttf 2.24.0).
	if (TextLineSkip line_skip{ GetParameter(TextLineSkip{}) };
		line_skip != std::numeric_limits<std::int32_t>::infinity()) {
		TTF_SetFontLineSkip(font, line_skip);
	}
#endif
	if (FontSize font_size{ GetParameter(FontSize{}) };
		font_size != std::numeric_limits<std::int32_t>::infinity()) {
		TTF_SetFontSize(font, font_size);
	}

	Color tc{ GetParameter(TextColor{}) };
	SDL_Color text_color{ tc.r, tc.g, tc.b, tc.a };

	FontRenderMode mode{ GetParameter(FontRenderMode{}) };
	TextWrapAfter wrap_after{ GetParameter(TextWrapAfter{}) };

	SDL_Surface* surface{ nullptr };

	switch (mode) {
		case FontRenderMode::Solid:
			surface = TTF_RenderUTF8_Solid_Wrapped(font, content.c_str(), text_color, wrap_after);
			break;
		case FontRenderMode::Shaded: {
			Color sc{ GetParameter(TextShadingColor{}) };
			SDL_Color shading_color{ sc.r, sc.g, sc.b, sc.a };
			surface = TTF_RenderUTF8_Shaded_Wrapped(
				font, content.c_str(), text_color, shading_color, wrap_after
			);
			break;
		}
		case FontRenderMode::Blended:
			surface = TTF_RenderUTF8_Blended_Wrapped(font, content.c_str(), text_color, wrap_after);
			break;
		default:
			PTGN_ERROR("Unrecognized render mode given when creating surface from font information"
			);
	}

	PTGN_ASSERT(surface != nullptr, "Failed to create surface for given font information");

	texture = impl::Texture(impl::Surface{ surface });
}

} // namespace ptgn