#include "runtime/graphics/text/font_system.h"

#include <ecs/ecs.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/graphics/surface.h"
#include "core/math/vector2.h"
#include "core/util/entity_handle.h"
#include "core/util/file.h"
#include "core/util/hash.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/text/font.h"
#include "runtime/graphics/text/text.h"

namespace ptgn {

FontSystem::FontSystem(AssetManager& assets) : assets_{ assets } {
	if (!raw_default_font_) {
		// TODO: Fix.
		// raw_default_font_ = GetRawBuffer(impl::GetLiberationSansRegular());
		// auto default_font{ LoadFromBinary(raw_default_font_, kDefaultFontSize, false) };
		impl::FontObject f{};

		Font font{ assets_.CreateAsset(), true };
		font.GetEntity().Add<FontSize>(kDefaultFontSize);
		font.GetEntity().Add<impl::FontObject>(std::move(f));
		impl::AddAssetKey(font.GetEntity(), "", std::nullopt);
	}
}

FontSystem::~FontSystem() noexcept {
	if (raw_default_font_) {
		// SDL_CloseIO(raw_default_font_);
	}
}

Font FontSystem::GetDefault() const {
	return assets_.Get<Font>(default_font_);
}

void FontSystem::SetDefault(std::string_view font_key) {
	PTGN_ASSERT(
		assets_.Has<Font>(font_key), "Font key must be loaded before setting it as default"
	);
	default_font_ = font_key;
}

int FontSystem::GetLineSkip(std::string_view font_key, FontSize font_size) const {
	// TODO: Fix.
	// return TTF_GetFontLineSkip(GetFont(font, font_size).get());
	return 0;
}

int FontSystem::GetHeight(std::string_view font_key, FontSize font_size) const {
	// TODO: Fix.
	// return TTF_GetFontHeight(GetFont(font, font_size).get());
	return 0;
}

V2_int FontSystem::GetSize(
	std::string_view font_key, std::string_view text_content, FontSize font_size, int max_wrap_width
) const {
	V2_int size;

	if (text_content.empty()) {
		size.x = 0;
		size.y = GetHeight(font_key, font_size);
		return size;
	}

	// TODO: Fix.
	// auto success{ TTF_GetStringSizeWrapped(
	//	GetFont(font, font_size).get(), text_content.data(), text_content.length(), max_wrap_width,
	//	&size.x, &size.y
	//) };
	// PTGN_ASSERT(success, "Failed to get size of wrapped font string");

	return size;
}

// TODO: Remove
// std::optional<impl::Surface> FontSystem::CreateTextSurface(
//	std::string_view text_content, Color color, FontSize font_size, Font font_asset,
//	const TextProperties& properties
//) {
// if (text_content.empty()) {
//	return std::nullopt;
// }
//// TODO: Fix.
// return std::nullopt;
/*
float scale{ hd_scale.value_or(1.0f) };

PTGN_ASSERT(font_asset.GetEntity().Has<std::shared_ptr<TTF_Font>>());

auto ttf_font{ font_asset.GetEntity().Get<std::shared_ptr<TTF_Font>>().get() };

font_size.GetValue() *= scale;

PTGN_ASSERT(ttf_font, "Cannot create texture for text with nullptr font");

TTF_SetFontStyle(ttf_font, std::to_underlying(properties.style));

TTF_SetFontWrapAlignment(
	ttf_font, static_cast<TTF_HorizontalAlignment>(std::to_underlying(properties.justify))
);

if (properties.line_skip.GetValue().has_value()) {
	auto line_skip{ static_cast<float>(*properties.line_skip.GetValue()) * scale };

	TTF_SetFontLineSkip(ttf_font, static_cast<int>(line_skip));
}

PTGN_ASSERT(font_size > 0, "Font size must be greater than zero");
PTGN_ASSERT(
	font_size < 10000, "Font size exceeds maximum allowable font size or grew recursively"
);

// NOSONAR
// V2_int dpi;
// auto success{ TTF_GetFontDPI(font, &dpi.x, &dpi.y) };
// PTGN_ASSERT(success, SDL_GetError());
// PTGN_LOG("[font=", font_asset, ",size=", font_size, ",dpi=", dpi);

TTF_SetFontSize(ttf_font, static_cast<float>(static_cast<int>(font_size)));

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
	TTF_SetFontOutline(ttf_font, outline_width);

	SDL_Color outline_color{ properties.outline.color.r, properties.outline.color.g,
							 properties.outline.color.b, properties.outline.color.a };

	outline_surface = TTF_RenderText_Blended_Wrapped(
		ttf_font, text_content.data(), text_content.length(), outline_color, wrap_after
	);

	PTGN_ASSERT(outline_surface, "Failed to create text outline");

	TTF_SetFontOutline(ttf_font, 0);
}

SDL_Surface* surface{ nullptr };

switch (properties.render_mode) {
	case FontRenderMode::Solid:
		surface = TTF_RenderText_Solid_Wrapped(
			ttf_font, text_content.data(), text_content.length(), text_color, wrap_after
		);
		break;
	case FontRenderMode::Shaded: {
		SDL_Color shading_color{ properties.shading_color.r, properties.shading_color.g,
								 properties.shading_color.b, properties.shading_color.a };
		surface = TTF_RenderText_Shaded_Wrapped(
			ttf_font, text_content.data(), text_content.length(), text_color, shading_color,
			wrap_after
		);
		break;
	}
	case FontRenderMode::Blended:
		surface = TTF_RenderText_Blended_Wrapped(
			ttf_font, text_content.data(), text_content.length(), text_color, wrap_after
		);
		break;
	default:
		PTGN_ERROR("Unrecognized render mode given when creating surface from font information"
		);
}

PTGN_ASSERT(surface, "Failed to create surface for given font information");

if (outline_surface) {
	SDL_Rect rect{ outline_width, outline_width, surface->w, surface->h };

	SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
	SDL_BlitSurface(surface, nullptr, outline_surface, &rect);
	SDL_DestroySurface(surface);

	surface = outline_surface;
}

PTGN_ASSERT(surface, "Failed to blit text surface to text outline surface");

return impl::Surface{ surface };
*/
//}

impl::FontObject FontSystem::CreateFont(
	impl::Renderer& renderer, const path& font_path, std::string_view name
) {
	PTGN_ASSERT(
		FileExists(font_path), "Cannot create font from invalid path: ", font_path.string()
	);

	auto cache_directory{ GetWorkingDirectory() / "cache/fonts" };

#ifndef __EMSCRIPTEN__

	auto cache_png_file{ cache_directory / (std::string(name) + ".png") };
	auto cache_data_file{ cache_directory / (std::string(name) + ".data") };

	if (FileExists(cache_png_file) && FileExists(cache_data_file)) {
		return impl::FontObject{ renderer, cache_directory, name };
	}
#endif

	return impl::FontObject{ renderer, font_path, cache_directory, name };
}

} // namespace ptgn