#include "runtime/asset/font_system.h"

#include <SDL3/SDL_blendmode.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/util/entity_handle.h"
#include "core/util/file.h"
#include "core/util/hash.h"
#include "ecs/ecs.h"
#include "renderer/image/surface.h"
#include "renderer/primitives/color.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/font.h"
#include "runtime/graphics/fonts.h"
#include "runtime/graphics/text.h"

#ifdef CreateFont
#undef CreateFont
#endif

namespace ptgn {

namespace impl {

void TTF_FontDeleter::operator()(TTF_Font* font) const {
	TTF_CloseFont(font);
}

} // namespace impl

static TTF_Font* LoadFromBinary(SDL_IOStream* raw_buffer, float font_size, bool free_buffer) {
	PTGN_ASSERT(raw_buffer != nullptr, SDL_GetError());
	auto ptr{ TTF_OpenFontIO(raw_buffer, free_buffer, font_size) };
	PTGN_ASSERT(ptr != nullptr, SDL_GetError());
	return ptr;
}

static SDL_IOStream* GetRawBuffer(const FontBinary& binary) {
	PTGN_ASSERT(binary.buffer != nullptr, "Cannot load font from invalid binary");
	return SDL_IOFromMem(
		static_cast<void*>(binary.buffer), static_cast<std::int32_t>(binary.length)
	);
}

FontSystem::FontSystem(AssetManager& assets) : assets_{ assets } {
	constexpr auto hash{ Hash(kDefaultFontKey) };
	if (!raw_default_font_) {
		raw_default_font_ = GetRawBuffer(impl::GetLiberationSansRegular());
		auto default_font{ LoadFromBinary(raw_default_font_, kDefaultFontSize, false) };
		std::shared_ptr<TTF_Font> f{ default_font, impl::TTF_FontDeleter{} };

		Font font{ assets_.CreateAsset(), true };
		font.GetEntity().Add<impl::FontSize>(kDefaultFontSize);
		font.GetEntity().Add<std::shared_ptr<TTF_Font>>(f);
		impl::AddAssetKey(font.GetEntity(), kDefaultFontKey, {});
	}
	default_font_key_ = hash;
}

FontSystem::~FontSystem() noexcept {
	if (raw_default_font_) {
		SDL_CloseIO(raw_default_font_);
	}
}

Font FontSystem::GetDefault() const {
	auto default_font{ assets_.GetFont(default_font_key_) };
	PTGN_ASSERT(default_font.has_value(), "Failed to find default font from asset manager");
	return *default_font;
}

void FontSystem::SetDefault(std::string_view key) {
	PTGN_ASSERT(assets_.HasFont(key), "Font key must be loaded before setting it as default");
	default_font_key_ = Hash(key);
}

std::shared_ptr<TTF_Font> FontSystem::GetFont(std::string_view key, std::optional<float> font_size)
	const {
	auto font{ assets_.GetFont(key) };

	if (!font) {
		return std::shared_ptr<TTF_Font>{ LoadFromBinary(raw_default_font_, *font_size, false),
										  impl::TTF_FontDeleter{} };
	}

	auto entity{ (*font).GetEntity() };

	if (!font_size.has_value()) {
		return entity.Get<std::shared_ptr<TTF_Font>>();
	}

	if (entity.Has<path>()) {
		auto path_string{ entity.Get<path>().string() };
		PTGN_ASSERT(!path_string.empty(), "Invalid font path");
		return std::shared_ptr<TTF_Font>{ TTF_OpenFont(path_string.c_str(), *font_size),
										  impl::TTF_FontDeleter{} };
	}

	// Font has no path defined.
	PTGN_ASSERT(
		entity.Get<impl::AssetKey>().hash == Hash(""),
		"Font key must have a valid path unless it is the default font"
	);

	return std::shared_ptr<TTF_Font>{ LoadFromBinary(raw_default_font_, *font_size, false),
									  impl::TTF_FontDeleter{} };
}

int FontSystem::GetLineSkip(std::string_view key, std::optional<float> font_size) const {
	return TTF_GetFontLineSkip(GetFont(key, font_size).get());
}

int FontSystem::GetHeight(std::string_view key, std::optional<float> font_size) const {
	return TTF_GetFontHeight(GetFont(key, font_size).get());
}

V2_int FontSystem::GetSize(
	std::string_view key, std::string_view text_content, std::optional<float> font_size,
	int max_wrap_width
) const {
	V2_int size;

	if (text_content.empty()) {
		size.x = 0;
		size.y = GetHeight(key, font_size);
		return size;
	}

	auto success{ TTF_GetStringSizeWrapped(
		GetFont(key, font_size).get(), text_content.data(), text_content.length(), max_wrap_width,
		&size.x, &size.y
	) };

	PTGN_ASSERT(success, "Failed to get size of wrapped font string");

	return size;
}

V2_int FontSystem::GetSize(
	Font font, std::string_view text_content, std::optional<float> font_size, int max_wrap_width
) const {
	return GetSize(
		font.GetEntity() ? font.GetEntity().Get<impl::AssetName>().name : "", text_content,
		font_size, max_wrap_width
	);
}

std::optional<impl::Surface> FontSystem::CreateTextSurface(
	std::string_view text_content, Color color, float font_size, Font font_asset,
	const TextProperties& properties, float hd_scale, bool hd
) const {
	if (text_content.empty()) {
		return {};
	}

	if (!font_asset) {
		font_asset = GetDefault();
	}

	float scale{ hd ? hd_scale : 1.0f };

	PTGN_ASSERT(font_asset);

	PTGN_ASSERT(font_asset.GetEntity().Has<std::shared_ptr<TTF_Font>>());

	auto font{ font_asset.GetEntity().Get<std::shared_ptr<TTF_Font>>().get() };

	PTGN_ASSERT(font != nullptr, "Cannot create texture for text with nullptr font");

	TTF_SetFontStyle(font, std::to_underlying(properties.style));

	TTF_SetFontWrapAlignment(
		font, static_cast<TTF_HorizontalAlignment>(std::to_underlying(properties.justify))
	);

	if (properties.line_skip.GetValue().has_value()) {
		auto line_skip{ static_cast<float>(*properties.line_skip.GetValue()) * scale };

		TTF_SetFontLineSkip(font, static_cast<int>(line_skip));
	}

	PTGN_ASSERT(font_size > 0, "Font size must be greater than zero");
	PTGN_ASSERT(
		font_size < 10000, "Font size exceeds maximum allowable font size or grew recursively"
	);

	// V2_int dpi;
	// auto success{ TTF_GetFontDPI(font, &dpi.x, &dpi.y) };
	// PTGN_ASSERT(success, SDL_GetError());
	// PTGN_LOG("[font=", font_asset, ",size=", font_size, ",dpi=", dpi);

	TTF_SetFontSize(font, static_cast<float>(static_cast<int>(font_size)));

	SDL_Color text_color{ color.r, color.g, color.b, color.a };

	auto outline_width{ static_cast<int>(properties.outline.width * scale) };

	PTGN_ASSERT(outline_width >= 0, "Cannot have negative font outline width");

	SDL_Surface* outline_surface{ nullptr };

	auto wrap_after{ static_cast<int>(properties.wrap_after * scale) };

	if (outline_width != 0 && properties.outline.color != color::Transparent) {
		PTGN_ASSERT(
			properties.render_mode == FontRenderMode::Blended,
			"Font render mode must be set to blended when drawing text with outline"
		);
		TTF_SetFontOutline(font, outline_width);

		SDL_Color outline_color{ properties.outline.color.r, properties.outline.color.g,
								 properties.outline.color.b, properties.outline.color.a };

		outline_surface = TTF_RenderText_Blended_Wrapped(
			font, text_content.data(), text_content.length(), outline_color, wrap_after
		);

		PTGN_ASSERT(outline_surface != nullptr, "Failed to create text outline");

		TTF_SetFontOutline(font, 0);
	}

	SDL_Surface* surface{ nullptr };

	switch (properties.render_mode) {
		case FontRenderMode::Solid:
			surface = TTF_RenderText_Solid_Wrapped(
				font, text_content.data(), text_content.length(), text_color, wrap_after
			);
			break;
		case FontRenderMode::Shaded: {
			SDL_Color shading_color{ properties.shading_color.r, properties.shading_color.g,
									 properties.shading_color.b, properties.shading_color.a };
			surface = TTF_RenderText_Shaded_Wrapped(
				font, text_content.data(), text_content.length(), text_color, shading_color,
				wrap_after
			);
			break;
		}
		case FontRenderMode::Blended:
			surface = TTF_RenderText_Blended_Wrapped(
				font, text_content.data(), text_content.length(), text_color, wrap_after
			);
			break;
		default:
			PTGN_ERROR("Unrecognized render mode given when creating surface from font information"
			);
	}

	PTGN_ASSERT(surface != nullptr, "Failed to create surface for given font information");

	if (outline_surface) {
		SDL_Rect rect{ outline_width, outline_width, surface->w, surface->h };

		SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
		SDL_BlitSurface(surface, nullptr, outline_surface, &rect);
		SDL_DestroySurface(surface);

		surface = outline_surface;
	}

	PTGN_ASSERT(surface != nullptr, "Failed to blit text surface to text outline surface");

	return impl::Surface{ surface };
}

std::shared_ptr<TTF_Font> FontSystem::CreateFont(const path& font_path, float pt_size) {
	PTGN_ASSERT(
		FileExists(font_path), "Cannot create font from invalid path: ", font_path.string()
	);

	auto ttf_font = TTF_OpenFont(font_path.string().c_str(), pt_size);

	PTGN_ASSERT(ttf_font, SDL_GetError());

	return std::shared_ptr<TTF_Font>{ ttf_font, impl::TTF_FontDeleter{} };
}

} // namespace ptgn