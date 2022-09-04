#include "protegon/font.h"

#include <cassert> // assert

#include <SDL.h>
#include <SDL_ttf.h>

#include "protegon/log.h"
#include "utility/file.h"

namespace ptgn {

Font::Font(const char* font_path, std::uint32_t point_size, std::uint32_t index) {
	assert(font_path != "" && "Empty font file path?");
	assert(FileExists(font_path) && "Nonexistent font file path?");
	font_ = TTF_OpenFontIndex(font_path, point_size, index);
	if (!IsValid()) {
		PrintLine(TTF_GetError());
		assert(!"Failed to create font");
	}
}

Font::~Font() {
	TTF_CloseFont(font_);
	font_ = nullptr;
}

std::int32_t Font::GetHeight() const {
	assert(IsValid() && "Cannot retrieve height of nonexistent font");
	return TTF_FontHeight(font_);
}

bool Font::IsValid() const {
	return font_ != nullptr;
}

} // namespace ptgn