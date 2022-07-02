#pragma once

#include "managers/SDLManager.h"
#include "texture/Texture.h"
#include "text/Text.h"
#include "text/Font.h"

namespace ptgn {

namespace internal {

namespace managers {

class TextureManager : public SDLManager<Texture> {};
class TextManager : public SDLManager<Text> {};
class FontManager : public SDLManager<Font> {};

} // namespace managers

} // namespace internal

} // namespace ptgn