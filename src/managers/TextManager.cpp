#include "TextManager.h"

#include "managers/SDLManager.h"
#include "managers/FontManager.h"
#include "managers/TextureManager.h"

namespace ptgn {

namespace internal {

namespace managers {

TextManager::TextManager() {
	GetSDLManager();
	GetFontManager();
	GetTextureManager();
}

} // namespace managers

managers::TextManager& GetTextManager() {
	static managers::TextManager text_manager;
	return text_manager;
}

} // namespace internal

} // namespace ptgn