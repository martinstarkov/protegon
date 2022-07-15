#pragma once

#include "managers/SDLManager.h"
#include "renderer/Texture.h"

namespace ptgn {

namespace managers {

class TextureManager : public SDLManager<Texture> {};

} // namespace managers

} // namespace ptgn