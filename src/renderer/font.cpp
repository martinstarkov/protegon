#include "protegon/font.h"

#include <cassert>

#include <SDL.h>
#include <SDL_ttf.h>

#include "protegon/log.h"

namespace ptgn {

Font::Font(const path& font_path, std::uint32_t point_size, std::uint32_t index) {
	assert(FileExists(font_path) && "Cannot create font from a nonexistent font path");
	instance_ = {
		TTF_OpenFontIndex(
			font_path.string().c_str(),
			point_size,
			index
		),
		TTF_CloseFont
	};
	if (!IsValid()) {
		PrintLine(TTF_GetError());
		assert(!"Failed to create font");
	}
}

std::int32_t Font::GetHeight() const {
	assert(IsValid() && "Cannot retrieve height of nonexistent font");
	return TTF_FontHeight(instance_.get());
}

} // namespace ptgn