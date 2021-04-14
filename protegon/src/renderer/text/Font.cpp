#include "Font.h"

#include <SDL_ttf.h>

#include <cassert>

namespace engine {

Font::Font(TTF_Font* font) : font{ font } {}

Font::Font(const char* file, std::uint32_t ptsize, std::uint32_t index) : font{ TTF_OpenFontIndex(file, ptsize, index) } {
	if (!IsValid()) {
		SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to create font: %s\n", TTF_GetError());
		assert(!true);
	}
}

TTF_Font* Font::operator=(TTF_Font* font) {
	this->font = font;
	return this->font;
}

Font::operator TTF_Font* () const {
	return font;
}

bool Font::IsValid() const {
	return font != nullptr;
}

TTF_Font* Font::operator&() const {
	return font;
}

void Font::Destroy() {
	TTF_CloseFont(font);
	font = nullptr;
}

} // namespace engine