#include "FontManager.h"

#include "managers/SDLManager.h"

namespace ptgn {

namespace internal {

namespace managers {

FontManager::FontManager() {
	GetSDLManager();
}

} // namespace managers

managers::FontManager& GetFontManager() {
	static managers::FontManager font_manager;
	return font_manager;
}

} // namespace internal

} // namespace ptgn