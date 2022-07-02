#include "Font.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include "debugging/Debug.h"

namespace ptgn {

namespace internal {

Font::Font(const char* font_path, std::uint32_t point_size, std::uint32_t index) {
	assert(font_path != "" && "Cannot load empty font path into the font manager");
	assert(debug::FileExists(font_path) && "Cannot load font with non-existent file path into the font manager");
	font_ = TTF_OpenFontIndex(font_path, point_size, index);
	if (font_ == nullptr) {
		debug::PrintLine(TTF_GetError());
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

Font::operator TTF_Font*() const {
	return font_;
}

} // namespace internal

} // namespace ptgn