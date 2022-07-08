#include "Font.h"

#include <cassert> // assert

#include <SDL.h>
#include <SDL_ttf.h>

#include "utility/File.h"
#include "utility/Log.h"

namespace ptgn {

Font::Font(const char* font_path, std::uint32_t point_size, std::uint32_t index) {
	assert(font_path != "" && "Cannot load empty font path into the font manager");
	assert(FileExists(font_path) && "Cannot load font with non-existent file path into the font manager");
	font_ = TTF_OpenFontIndex(font_path, point_size, index);
	if (font_ == nullptr) {
		PrintLine(TTF_GetError());
		assert(!"Failed to load font into the font manager");
	}
}

Font::~Font() {
	TTF_CloseFont(font_);
	font_ = nullptr;
}

std::int32_t Font::GetHeight() const {
	return TTF_FontHeight(font_);
}

} // namespace ptgn