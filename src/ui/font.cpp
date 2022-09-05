#include "protegon/font.h"

#include <cassert> // assert

#include <SDL.h>
#include <SDL_ttf.h>

#include "protegon/log.h"
#include "utility/file.h"

namespace ptgn {

void Font::TTF_Font_Deleter::operator()(TTF_Font* font) {
	TTF_CloseFont(font);
}

Font::Font(const char* font_path, std::uint32_t point_size, std::uint32_t index) {
	assert(font_path != "" && "Empty font file path?");
	assert(FileExists(font_path) && "Nonexistent font file path?");
	font_ = std::make_shared<TTF_Font>(TTF_OpenFontIndex(font_path, point_size, index), TTF_Font_Deleter{});
	if (!IsValid()) {
		PrintLine(TTF_GetError());
		assert(!"Failed to create font");
	}
}

std::int32_t Font::GetHeight() const {
	assert(IsValid() && "Cannot retrieve height of nonexistent font");
	return TTF_FontHeight(font_.get());
}

bool Font::IsValid() const {
	return font_ != nullptr;
}

} // namespace ptgn