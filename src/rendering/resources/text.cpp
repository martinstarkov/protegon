#include "rendering/resources/text.h"

#include <cstdint>
#include <limits>
#include <string>

#include "SDL_blendmode.h"
#include "SDL_pixels.h"
#include "SDL_rect.h"
#include "SDL_surface.h"
#include "SDL_ttf.h"
#include "common/assert.h"
#include "components/draw.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "debug/log.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "rendering/batching/render_data.h"
#include "rendering/resources/font.h"
#include "rendering/resources/texture.h"
#include "scene/camera.h"
#include "scene/scene.h"

namespace ptgn {

Text CreateText(
	Scene& scene, const TextContent& content, const TextColor& text_color, const FontKey& font_key
) {
	Text text{ scene.CreateEntity() };
	text.Add<TextureHandle>();
	text.SetDraw<Text>();
	text.Add<Camera>(scene.camera.window_unzoomed);
	text.Show();
	text.SetParameter(content, false);
	text.SetParameter(text_color, false);
	text.SetParameter(font_key, false);
	text.RecreateTexture();

	return text;
}

Text::Text(const Entity& entity) : Entity{ entity } {}

void Text::Draw(impl::RenderData& ctx, const Entity& entity) {
	if (entity.Has<TextColor>() && entity.Get<TextColor>().a == 0) {
		return;
	}

	if (!entity.Has<TextContent>()) {
		return;
	}

	if (std::string_view{ entity.Get<TextContent>() }.empty()) {
		return;
	}

	Sprite::Draw(ctx, entity);
}

Text& Text::SetFont(const FontKey& font_key) {
	return SetParameter(font_key);
}

Text& Text::SetContent(const TextContent& content) {
	return SetParameter(content);
}

Text& Text::SetColor(const TextColor& color) {
	return SetParameter(color);
}

Text& Text::SetFontStyle(FontStyle font_style) {
	return SetParameter(font_style);
}

Text& Text::SetFontSize(std::int32_t pixels) {
	return SetParameter(FontSize{ pixels });
}

Text& Text::SetOutline(std::int32_t width, const Color& color) {
	SetParameter(FontRenderMode::Blended, false);
	return SetParameter(TextOutline{ width, color });
}

Text& Text::SetFontRenderMode(FontRenderMode render_mode) {
	return SetParameter(render_mode);
}

Text& Text::SetShadingColor(const Color& shading_color) {
	SetParameter(FontRenderMode::Shaded, false);
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

FontKey Text::GetFontKey() const {
	return GetParameter(FontKey{});
}

TextContent Text::GetContent() const {
	return GetParameter(TextContent{});
}

TextColor Text::GetColor() const {
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
	return GetSize(*this);
}

V2_int Text::GetSize(const Entity& text) {
	return GetSize(GetParameter(text, TextContent{}), GetParameter(text, FontKey{}));
}

V2_int Text::GetSize(const std::string& content, const FontKey& font_key) {
	PTGN_ASSERT(
		game.font.Has(font_key),
		"Cannot get size of text texture unless its font is loaded in the font manager"
	);
	return game.font.GetSize(font_key, content);
}

impl::Texture Text::CreateTexture(
	const std::string& content, const TextColor& color, const FontSize& font_size,
	const FontKey& font_key, const TextProperties& properties
) {
	if (content.empty()) {
		return {};
	}

	PTGN_ASSERT(
		game.font.Has(font_key),
		"Cannot create texture for text with font key which is not loaded in the font manager"
	);

	auto font{ game.font.Get(font_key) };

	PTGN_ASSERT(font != nullptr, "Cannot create texture for text with nullptr font");

	TTF_SetFontStyle(font, static_cast<int>(properties.style));

	TTF_SetFontWrappedAlign(font, static_cast<int>(properties.justify));

#ifndef __EMSCRIPTEN__ // TODO: Re-enable this for Emscripten once it is supported (SDL_ttf 2.24.0).
	if (properties.line_skip != std::numeric_limits<std::int32_t>::infinity()) {
		TTF_SetFontLineSkip(font, properties.line_skip);
	}
#endif
	if (font_size != std::numeric_limits<std::int32_t>::infinity()) {
		TTF_SetFontSize(font, font_size);
	}

	SDL_Color text_color{ color.r, color.g, color.b, color.a };

	PTGN_ASSERT(properties.outline.width >= 0, "Cannot have negative font outline width");

	SDL_Surface* outline_surface{ nullptr };

	if (properties.outline.width != 0 && properties.outline.color != color::Transparent) {
		PTGN_ASSERT(
			properties.render_mode == FontRenderMode::Blended,
			"Font render mode must be set to blended when drawing text with outline"
		);
		TTF_SetFontOutline(font, properties.outline.width);

		SDL_Color outline_color{ properties.outline.color.r, properties.outline.color.g,
								 properties.outline.color.b, properties.outline.color.a };

		outline_surface = TTF_RenderUTF8_Blended_Wrapped(
			font, content.c_str(), outline_color, properties.wrap_after
		);

		PTGN_ASSERT(outline_surface != nullptr, "Failed to create text outline");

		TTF_SetFontOutline(font, 0);
	}

	SDL_Surface* surface{ nullptr };

	switch (properties.render_mode) {
		case FontRenderMode::Solid:
			surface = TTF_RenderUTF8_Solid_Wrapped(
				font, content.c_str(), text_color, properties.wrap_after
			);
			break;
		case FontRenderMode::Shaded: {
			SDL_Color shading_color{ properties.shading_color.r, properties.shading_color.g,
									 properties.shading_color.b, properties.shading_color.a };
			surface = TTF_RenderUTF8_Shaded_Wrapped(
				font, content.c_str(), text_color, shading_color, properties.wrap_after
			);
			break;
		}
		case FontRenderMode::Blended:
			surface = TTF_RenderUTF8_Blended_Wrapped(
				font, content.c_str(), text_color, properties.wrap_after
			);
			break;
		default:
			PTGN_ERROR(
				"Unrecognized render mode given when creating surface from font information"
			);
	}

	PTGN_ASSERT(surface != nullptr, "Failed to create surface for given font information");

	if (outline_surface) {
		SDL_Rect rect{ properties.outline.width, properties.outline.width, surface->w, surface->h };

		SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
		SDL_BlitSurface(surface, NULL, outline_surface, &rect);
		SDL_FreeSurface(surface);

		surface = outline_surface;
	}

	PTGN_ASSERT(surface != nullptr, "Failed to blit text surface to text outline surface");

	return impl::Texture(impl::Surface{ surface });
}

void Text::RecreateTexture() {
	// TODO: Move texture location to TextureManager.
	impl::Texture& texture{ Has<impl::Texture>() ? Get<impl::Texture>() : Add<impl::Texture>() };

	TextProperties properties;

	properties.justify		 = GetParameter(TextJustify{});
	properties.line_skip	 = GetParameter(TextLineSkip{});
	properties.outline		 = GetParameter(TextOutline{});
	properties.render_mode	 = GetParameter(FontRenderMode{});
	properties.shading_color = GetParameter(TextShadingColor{});
	properties.style		 = GetParameter(FontStyle{});
	properties.wrap_after	 = GetParameter(TextWrapAfter{});

	texture = CreateTexture(
		GetParameter(TextContent{}), GetParameter(TextColor{}), GetParameter(FontSize{}),
		GetParameter(FontKey{}), properties
	);
}

} // namespace ptgn