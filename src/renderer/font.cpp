#include "renderer/font.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <variant>

#include "SDL_error.h"
#include "SDL_rwops.h"
#include "SDL_ttf.h"
#include "core/game.h"
#include "core/manager.h"
#include "core/sdl_instance.h"
#include "resources/fonts.h"
#include "utility/debug.h"
#include "utility/file.h"
#include "utility/handle.h"

namespace ptgn {

namespace impl {

struct TTF_FontDeleter {
	void operator()(TTF_Font* font) const {
		if (game.sdl_instance_->SDLTTFIsInitialized()) {
			TTF_CloseFont(font);
		}
	}
};

Font FontManager::GetFontOrKey(const FontOrKey& font) const {
	Font f;
	if (std::holds_alternative<impl::FontManager::Key>(font)) {
		const auto& font_key{ std::get<impl::FontManager::Key>(font) };
		PTGN_ASSERT(Has(font_key), "Load font into manager before using it");
		f = Get(font_key);
	} else if (std::holds_alternative<impl::FontManager::InternalKey>(font)) {
		const auto& font_key{ std::get<impl::FontManager::InternalKey>(font) };
		PTGN_ASSERT(Has(font_key), "Load font into manager before using it");
		f = Get(font_key);
	} else if (std::holds_alternative<Font>(font)) {
		f = std::get<Font>(font);
	}
	PTGN_ASSERT(f.IsValid(), "Cannot retrieve font with an invalid pointer");
	return f;
}

void FontManager::Init() {
	Font initial_font{ font::LiberationSansRegular, 20 };
	PTGN_ASSERT(initial_font.IsValid(), "Failed to retrieve initial default font");
	default_font_ = initial_font;
}

void FontManager::SetDefault(const FontOrKey& font) {
	Font f;
	if (font == FontOrKey{}) {
		f = Font{ font::LiberationSansRegular, 20 };
	} else {
		f = GetFontOrKey(font);
	};
	PTGN_ASSERT(f.IsValid(), "Cannot set default font to invalid font");
	default_font_ = f;
}

Font FontManager::GetDefault() const {
	PTGN_ASSERT(
		default_font_.IsValid(), "Cannot retrieve default font before game has been started"
	);
	return default_font_;
}

} // namespace impl

Font::Font(const path& font_path, std::int32_t point_size, std::int32_t index) {
	PTGN_ASSERT(point_size > 0, "Cannot load font with point size <= 0");
	PTGN_ASSERT(index >= 0, "Cannot load font with negative index");
	PTGN_ASSERT(
		FileExists(font_path),
		"Cannot create font from a nonexistent font path: ", font_path.string()
	);
	Create({ TTF_OpenFontIndex(font_path.string().c_str(), point_size, index),
			 impl::TTF_FontDeleter{} });
	PTGN_ASSERT(IsValid(), TTF_GetError());
}

Font::Font(const impl::FontBinary& font, std::int32_t point_size) {
	PTGN_ASSERT(point_size > 0, "Cannot load font with point size <= 0");
	PTGN_ASSERT(font.buffer != nullptr, "Cannot create font from invalid font binary");
	SDL_RWops* rw = SDL_RWFromMem((void*)font.buffer, font.length);
	PTGN_ASSERT(rw != nullptr, SDL_GetError());
	Create({ TTF_OpenFontRW(rw, 1, point_size), impl::TTF_FontDeleter{} });
	PTGN_ASSERT(IsValid(), TTF_GetError());
}

std::int32_t Font::GetHeight() const {
	return TTF_FontHeight(&Get());
}

} // namespace ptgn