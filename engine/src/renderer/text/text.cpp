#include "renderer/text/text.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <string>

#include "SDL_blendmode.h"
#include "SDL_pixels.h"
#include "SDL_rect.h"
#include "SDL_surface.h"
#include "SDL_ttf.h"
#include "core/app/manager.h"
#include "core/assert.h"
#include "core/asset/asset_manager.h"
#include "core/ecs/components/draw.h"
#include "core/ecs/components/effects.h"
#include "core/ecs/components/generic.h"
#include "core/ecs/components/sprite.h"
#include "core/ecs/components/transform.h"
#include "core/ecs/entity.h"
#include "core/log.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/material/texture.h"
#include "renderer/render_data.h"
#include "renderer/text/font.h"
#include "world/scene/camera.h"
#include "world/scene/scene.h"

namespace ptgn {

Text::Text(const Entity& entity) : Entity{ entity } {}

void Text::Draw(const Entity& entity) {
	impl::DrawText(entity);
}

Text& Text::SetFont(const ResourceHandle& font_key) {
	SetParameter(font_key);
	return *this;
}

Text& Text::SetContent(const TextContent& content) {
	SetParameter(content);
	return *this;
}

Text& Text::SetColor(const TextColor& color) {
	SetParameter(color);
	return *this;
}

Text& Text::SetFontStyle(FontStyle font_style) {
	SetParameter(font_style);
	return *this;
}

Text& Text::SetFontSize(const FontSize& pixels) {
	SetParameter(pixels);
	return *this;
}

Text& Text::SetOutline(const TextOutline& outline) {
	SetParameter(FontRenderMode::Blended, false);
	SetParameter(outline, true);
	return *this;
}

Text& Text::SetFontRenderMode(FontRenderMode render_mode) {
	SetParameter(render_mode);
	return *this;
}

Text& Text::SetShadingColor(const Color& shading_color) {
	SetParameter(FontRenderMode::Shaded, false);
	SetParameter(TextShadingColor{ shading_color }, true);
	return *this;
}

Text& Text::SetWrapAfter(const TextWrapAfter& pixels) {
	SetParameter(pixels);
	return *this;
}

Text& Text::SetLineSkip(const TextLineSkip& pixels) {
	SetParameter(pixels);
	return *this;
}

Text& Text::SetTextJustify(TextJustify text_justify) {
	SetParameter(text_justify);
	return *this;
}

ResourceHandle Text::GetFontKey() const {
	return GetParameter(ResourceHandle{});
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

FontSize Text::GetFontSize(bool hd, const Camera& camera) const {
	FontSize font_size{ GetParameter(FontSize{}) };
	if (hd) {
		const auto& scene{ GetScene() };
		auto cam{ camera ? camera : GetCamera() };
		return font_size.GetHD(scene, cam);
	}
	return font_size;
}

V2_int Text::GetSize(const Camera& camera) const {
	return GetSize(*this, camera);
}

V2_int Text::GetSize(const TextContent& content, const Camera& camera) const {
	return GetSize(content, GetFontKey(), GetFontSize(IsHD(), camera));
}

V2_int Text::GetSize(const Entity& text, const Camera& camera) {
	Text t{ text };
	return GetSize(
		GetParameter(text, TextContent{}), GetParameter(text, ResourceHandle{}),
		t.GetFontSize(t.IsHD(), camera)
	);
}

V2_int Text::GetSize(
	const TextContent& content, const ResourceHandle& font_key, const FontSize& font_size
) {
	// TODO: FIX THIS TO USE ASSET MANAGER.
	PTGN_ASSERT(false);
	return {};
	// PTGN_ASSERT(
	//	Application::Get().font.Has(font_key),
	//	"Cannot get size of text texture unless its font is loaded in the font manager"
	//);
	// return Application::Get().font.GetSize(font_key, content, font_size);
}

impl::Texture Text::CreateTexture(const FontSize& font_size) const {
	TextContent content{ GetContent() };
	TextColor color{ GetColor() };
	ResourceHandle font_key{ GetFontKey() };
	TextProperties properties{ GetProperties() };

	return CreateTexture(content, color, font_size, font_key, properties);
}

impl::Texture Text::CreateTexture(
	const TextContent& content, const TextColor& color, const FontSize& font_size,
	const ResourceHandle& font_key, const TextProperties& properties
) {
	const auto& text_content{ content.GetValue() };

	if (text_content.empty()) {
		return {};
	}

	// TODO: Fix these to use asset manager.

	// PTGN_ASSERT(
	//	Application::Get().font.Has(font_key),
	//	"Cannot create texture for text with font key which is not loaded in the font manager"
	//);

	// auto shared_font{ Application::Get().font.Get(
	//	font_key, {} /* Force retrieval of the font regardless of size since this function also sets
	//					the font size. */
	//) };
	// auto font{ shared_font.get() };

	TTF_Font* font{ nullptr };

	PTGN_ASSERT(font != nullptr, "Cannot create texture for text with nullptr font");

	TTF_SetFontStyle(font, static_cast<int>(properties.style));

	TTF_SetFontWrappedAlign(font, static_cast<int>(properties.justify));

#ifndef __EMSCRIPTEN__ // TODO: Re-enable this for Emscripten once it is supported (SDL_ttf 2.24.0).
	if (properties.line_skip != std::numeric_limits<std::int32_t>::infinity()) {
		TTF_SetFontLineSkip(font, properties.line_skip);
	}
#endif

	PTGN_ASSERT(font_size > 0, "Font size must be greater than zero");
	PTGN_ASSERT(
		font_size < 10000, "Font size exceeds maximum allowable font size or grew recursively"
	);

	TTF_SetFontSize(font, font_size);

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
			font, text_content.c_str(), outline_color, properties.wrap_after
		);

		PTGN_ASSERT(outline_surface != nullptr, "Failed to create text outline");

		TTF_SetFontOutline(font, 0);
	}

	SDL_Surface* surface{ nullptr };

	switch (properties.render_mode) {
		case FontRenderMode::Solid:
			surface = TTF_RenderUTF8_Solid_Wrapped(
				font, text_content.c_str(), text_color, properties.wrap_after
			);
			break;
		case FontRenderMode::Shaded: {
			SDL_Color shading_color{ properties.shading_color.r, properties.shading_color.g,
									 properties.shading_color.b, properties.shading_color.a };
			surface = TTF_RenderUTF8_Shaded_Wrapped(
				font, text_content.c_str(), text_color, shading_color, properties.wrap_after
			);
			break;
		}
		case FontRenderMode::Blended:
			surface = TTF_RenderUTF8_Blended_Wrapped(
				font, text_content.c_str(), text_color, properties.wrap_after
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

Text& Text::SetHD(bool hd, const Camera& camera) {
	if (hd == IsHD()) {
		return *this;
	}
	Add<impl::HDText>(hd);
	RecreateTexture(camera);
	return *this;
}

bool Text::IsHD() const {
	return Has<impl::HDText>() && Get<impl::HDText>();
}

void Text::RecreateTexture(const Camera& camera) {
	TextContent content{ GetContent() };
	TextColor color{ GetColor() };
	FontSize font_size{ GetFontSize(IsHD(), camera) };
	ResourceHandle font_key{ GetFontKey() };
	TextProperties properties{ GetProperties() };

	RecreateTexture(content, color, font_size, font_key, properties);
}

void Text::RecreateTexture(
	const TextContent& content, const TextColor& color, const FontSize& font_size,
	const ResourceHandle& font_key, const TextProperties& properties
) {
	// Cache the font size of the texture so that if HD resolution changes, the text is updated
	// before drawing.
	Add<impl::CachedFontSize>(font_size);

	// TODO: Move texture location to TextureManager.
	impl::Texture& texture{ TryAdd<impl::Texture>() };

	texture = CreateTexture(content, color, font_size, font_key, properties);
}

TextProperties Text::GetProperties() const {
	TextProperties properties;
	properties.justify		 = GetParameter(TextJustify{});
	properties.line_skip	 = GetParameter(TextLineSkip{});
	properties.outline		 = GetParameter(TextOutline{});
	properties.render_mode	 = GetParameter(FontRenderMode{});
	properties.shading_color = GetParameter(TextShadingColor{});
	properties.style		 = GetParameter(FontStyle{});
	properties.wrap_after	 = GetParameter(TextWrapAfter{});
	return properties;
}

void Text::SetProperties(const TextProperties& properties, const Camera& camera) {
	SetProperties(properties, true, camera);
}

void Text::SetProperties(
	const TextProperties& properties, bool recreate_texture, const Camera& camera
) {
	bool changed  = false;
	changed		 |= SetParameter(properties.justify, false);
	changed		 |= SetParameter(properties.line_skip, false);
	changed		 |= SetParameter(properties.outline, false);
	changed		 |= SetParameter(properties.render_mode, false);
	changed		 |= SetParameter(properties.shading_color, false);
	changed		 |= SetParameter(properties.style, false);
	changed		 |= SetParameter(properties.wrap_after, false);

	if (changed && recreate_texture) {
		RecreateTexture(camera);
	}
}

Text CreateText(
	Manager& manager, const TextContent& content, const TextColor& text_color,
	const FontSize& font_size, const ResourceHandle& font_key, const TextProperties& properties
) {
	Text text{ manager.CreateEntity() };
	text.Add<TextureHandle>();
	SetDraw<Text>(text);
	Show(text);
	text.Add<impl::HDText>(true);
	text.SetParameter(content, false);
	text.SetParameter(text_color, false);
	text.SetParameter(font_key, false);
	text.SetParameter(font_size, false);
	text.SetProperties(properties, true, {});
	return text;
}

} // namespace ptgn