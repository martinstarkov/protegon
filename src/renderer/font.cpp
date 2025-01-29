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

namespace ptgn::impl {

struct TTF_FontDeleter {
	void operator()(TTF_Font* font) const {
		if (game.sdl_instance_->SDLTTFIsInitialized()) {
			TTF_CloseFont(font);
		}
	}
};

void FontManager::Init() {
	FontInstance initial_font{ font::LiberationSansRegular, 20 };
	// default_font_ = initial_font;
}

// void FontManager::SetDefault(const FontOrKey& font) {
//	default_font_ = font;
// }

// Font FontManager::GetDefault() const {
//	return default_font_;
// }

FontInstance::FontInstance(const path& font_path, std::int32_t point_size, std::int32_t index) {
	PTGN_ASSERT(point_size > 0, "Cannot load font with point size <= 0");
	PTGN_ASSERT(index >= 0, "Cannot load font with negative index");
	PTGN_ASSERT(
		FileExists(font_path),
		"Cannot create font from a nonexistent font path: ", font_path.string()
	);
	font_ = { TTF_OpenFontIndex(font_path.string().c_str(), point_size, index),
			  impl::TTF_FontDeleter{} };
	PTGN_ASSERT(font_ != nullptr, TTF_GetError());
}

FontInstance::FontInstance(const impl::FontBinary& font, std::int32_t point_size) {
	PTGN_ASSERT(point_size > 0, "Cannot load font with point size <= 0");
	PTGN_ASSERT(font.buffer != nullptr, "Cannot create font from invalid font binary");
	SDL_RWops* rw =
		SDL_RWFromMem(static_cast<void*>(font.buffer), static_cast<std::int32_t>(font.length));
	PTGN_ASSERT(rw != nullptr, SDL_GetError());
	font_ = { TTF_OpenFontRW(rw, 1, point_size), impl::TTF_FontDeleter{} };
	PTGN_ASSERT(font_ != nullptr, TTF_GetError());
}

std::int32_t FontInstance::GetHeight() const {
	return TTF_FontHeight(font_.get());
}

} // namespace ptgn::impl