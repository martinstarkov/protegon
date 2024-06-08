#include "protegon/font.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include "utility/debug.h"

namespace ptgn {

Font::Font(const path& font_path, std::uint32_t point_size, std::uint32_t index) {
	PTGN_CHECK(FileExists(font_path), "Cannot create font from a nonexistent font path");
	instance_ = {
		TTF_OpenFontIndex(
			font_path.string().c_str(),
			point_size,
			index
		),
		TTF_CloseFont
	};
	if (!IsValid()) {
		PTGN_ERROR(TTF_GetError());
		PTGN_ASSERT(false, "Failed to create font");
	}
}

std::int32_t Font::GetHeight() const {
	PTGN_CHECK(IsValid(), "Cannot retrieve height of nonexistent font");
	return TTF_FontHeight(instance_.get());
}

} // namespace ptgn