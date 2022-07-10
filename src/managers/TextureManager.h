#pragma once

#include "managers/ResourceManager.h"
#include "renderer/Texture.h"

namespace ptgn {

namespace managers {

class TextureManager : public ResourceManager<Texture> {};

} // namespace managers

} // namespace ptgn