#include "protegon/font.h"

#include "protegon/game.h"
#include "SDL.h"
#include "SDL_ttf.h"
#include "utility/debug.h"

namespace ptgn {

namespace impl {

struct TTF_FontDeleter {
	void operator()(TTF_Font* font) {
		if (game.sdl_instance_.SDLTTFIsInitialized()) {
			TTF_CloseFont(font);
		}
	}
};

} // namespace impl

Font::Font(const path& font_path, std::int32_t point_size, std::int32_t index) {
	PTGN_ASSERT(point_size > 0, "Cannot load font with point size <= 0");
	PTGN_ASSERT(index >= 0, "Cannot load font with negative index");
	PTGN_ASSERT(
		FileExists(font_path),
		"Cannot create font from a nonexistent font path: ", font_path.string()
	);
	instance_ = { TTF_OpenFontIndex(font_path.string().c_str(), point_size, index),
				  impl::TTF_FontDeleter{} };
	PTGN_ASSERT(IsValid(), TTF_GetError());
}

std::int32_t Font::GetHeight() const {
	PTGN_ASSERT(IsValid(), "Cannot retrieve height of nonexistent font");
	return TTF_FontHeight(instance_.get());
}

} // namespace ptgn