#include "Font.h"

#include <cassert> // assert

#include <SDL.h>
#include <SDL_ttf.h>

#include "utility/File.h"
#include "utility/Log.h"

namespace ptgn {

Font::Font(const char* font_path, std::uint32_t point_size, std::uint32_t index) {
	assert(font_path != "" && "Cannot load empty font path into the font manager");
	assert(FileExists(font_path) && "Cannot load font with nonexistent file path into the font manager");
	font_ = TTF_OpenFontIndex(font_path, point_size, index);
	if (!Exists()) {
		PrintLine(TTF_GetError());
		assert(!"Failed to load font into the font manager");
	}
}

Font::~Font() {
	TTF_CloseFont(font_);
	font_ = nullptr;
}

std::int32_t Font::GetHeight() const {
	assert(Exists() && "Cannot retrieve height of nonexistent font");
	return TTF_FontHeight(font_);
}

} // namespace ptgn