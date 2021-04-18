#include "Font.h"

#include <SDL_ttf.h>

#include <cassert>
#include <iostream>

namespace engine {

Font::Font(TTF_Font* font) : font_{ font } {}

Font::Font(const char* file, std::uint32_t ptsize, std::uint32_t index) : font_{ TTF_OpenFontIndex(file, ptsize, index) } {
	if (!IsValid()) {
		std::cout << "Failed to create font: " << TTF_GetError() << std::endl;
		assert(!true);
	}
}

Font::operator TTF_Font* () const {
	return font_;
}

TTF_Font* Font::operator&() const {
	return font_;
}

std::int32_t Font::GetMaxPixelHeight() const {
	return TTF_FontHeight(font_);
}

bool Font::IsValid() const {
	return font_ != nullptr;
}

void Font::Destroy() {
	TTF_CloseFont(font_);
	font_ = nullptr;
}

} // namespace engine