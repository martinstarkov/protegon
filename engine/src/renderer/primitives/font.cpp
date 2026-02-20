#include "renderer/primitives/font.h"

#include <SDL3_ttf/SDL_ttf.h>

namespace ptgn {

namespace impl {

void TTF_FontDeleter::operator()(TTF_Font* font) const {
	TTF_CloseFont(font);
}

} // namespace impl

} // namespace ptgn